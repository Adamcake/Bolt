#include "client.hxx"

#include "include/cef_life_span_handler.h"
#include "include/cef_app.h"
#include "include/cef_values.h"
#include "include/internal/cef_types.h"
#include "resource_handler.hxx"
#include "window_launcher.hxx"

#if defined(BOLT_PLUGINS)
#include "include/cef_parser.h"
#include "../library/ipc.h"
#include <new>
#include <cstring>
#define BOLT_IPC_URL "https://bolt-blankpage/"
#endif

#include <algorithm>
#include <fcntl.h>
#include <fmt/core.h>

#if defined(CEF_X11)
#include <xcb/xcb.h>
#endif

#if defined(BOLT_DEV_SHOW_DEVTOOLS)
constexpr bool SHOW_DEVTOOLS = true;
#else
constexpr bool SHOW_DEVTOOLS = false;
#endif

constexpr Browser::Details LAUNCHER_DETAILS = {
	.preferred_width = 800,
	.preferred_height = 608,
	.center_on_open = true,
	.resizeable = true,
	.frame = true,
};

#if defined(BOLT_PLUGINS)
bool ReadPlugins(const CefRefPtr<CefValue> config, std::vector<BoltPlugin>& plugins) {
	CefRefPtr<CefDictionaryValue> list = config->GetDictionary();
	if (!list) {
		fmt::print("[B] error: plugins.json: base element must be a dictionary\n");
		return false;
	}
	CefDictionaryValue::KeyList keys;
	if (!list->GetKeys(keys)) {
		fmt::print("[B] error: plugins.json: failed to parse key list\n");
		return false;
	}
	for (const CefString& plugin_id: keys) {
		BoltPlugin plugin;
		CefRefPtr<CefDictionaryValue> item = list->GetDictionary(plugin_id);
		if (!item) {
			fmt::print("[B] error: plugins.json: expected value of '{}' to be a dictionary\n", plugin_id.ToString());
			return false;
		}
		if (!item->HasKey("name") || !item->HasKey("path") || !item->HasKey("main")) {
			fmt::print("[B] error: plugins.json: at least one required key 'name', 'path', 'main' is missing from '{}'\n", plugin_id.ToString());
			return false;
		}
		plugin.id = plugin_id;
		plugin.has_desc = item->HasKey("desc");
		plugin.name = item->GetString("name");
		if (plugin.has_desc) plugin.desc = item->GetString("desc");
		plugin.path = item->GetString("path");
		plugin.main = item->GetString("main");
		plugins.push_back(plugin);
	}
	return true;
}
#endif

Browser::Client::Client(CefRefPtr<Browser::App> app,std::filesystem::path config_dir, std::filesystem::path data_dir, std::filesystem::path runtime_dir):
#if defined(BOLT_DEV_LAUNCHER_DIRECTORY)
	CLIENT_FILEHANDLER(BOLT_DEV_LAUNCHER_DIRECTORY),
#endif
#if defined(BOLT_PLUGINS)
	any_clients(false),
#endif
	show_devtools(SHOW_DEVTOOLS), config_dir(config_dir), data_dir(data_dir), runtime_dir(runtime_dir)
{
	app->SetBrowserProcessHandler(this);

#if defined(BOLT_PLUGINS)
	std::filesystem::path plugins_conf = config_dir;
	plugins_conf.append("plugins.json");
	std::ifstream ifs(plugins_conf.c_str(), std::ios::in | std::ios::binary);
	if (!ifs.fail()) {
		std::stringstream ss;
		ss << ifs.rdbuf();
		std::vector<BoltPlugin> plugins;
		CefRefPtr<CefValue> plugin_info = CefParseJSON(CefString(ss.str()), JSON_PARSER_ALLOW_TRAILING_COMMAS);
		if (ReadPlugins(plugin_info, plugins)) {
			this->plugins = plugins;
		} else {
			fmt::print("[B] config file plugins.json was ignored as it contained an error\n");
		}
	}
	
	this->IPCBind();
	this->ipc_thread = std::thread(&Browser::Client::IPCRun, this);
#endif

#if defined(CEF_X11)
	this->xcb = xcb_connect(nullptr, nullptr);
#endif
}

void Browser::Client::OpenLauncher() {
	std::lock_guard<std::mutex> _(this->windows_lock);
	auto it = std::find_if(this->windows.begin(), this->windows.end(), [](CefRefPtr<Window>& win) { return win->IsLauncher(); });
	if (it == this->windows.end()) {
		CefRefPtr<Window> w = new Browser::Launcher(this, LAUNCHER_DETAILS, this->show_devtools, this, this->config_dir, this->data_dir);
		this->windows.push_back(w);
	} else {
		(*it)->Focus();
	}
}

void Browser::Client::TryExit() {
	this->windows_lock.lock();
	size_t window_count = this->windows.size();
	this->windows_lock.unlock();

#if defined(BOLT_PLUGINS)
	bool can_exit = (window_count == 0) && !this->any_clients;
	fmt::print("[B] TryExit, windows={}, any_clients={}, exit={}\n", window_count, this->any_clients, can_exit);
#else
	bool can_exit = window_count == 0;
	fmt::print("[B] TryExit, windows={}, exit={}\n", window_count, can_exit);
#endif
	if (can_exit) {
#if defined(BOLT_PLUGINS)
		this->ipc_window->Close();
#else
		this->Exit();
#endif
	}
}

void Browser::Client::Exit() {
	this->StopFileManager();
#if defined(CEF_X11)
	xcb_disconnect(this->xcb);
#endif
#if defined(BOLT_PLUGINS)
	this->IPCStop();
#endif
	CefQuitMessageLoop();
}

void Browser::Client::OnBoltWindowCreated(CefRefPtr<CefWindow> window) {
	CefRefPtr<CefImage> image = CefImage::CreateImage();
	image->AddBitmap(1.0, 16, 16, CEF_COLOR_TYPE_RGBA_8888, CEF_ALPHA_TYPE_POSTMULTIPLIED, this->GetIcon16(), 16 * 16 * 4);
	window->SetWindowIcon(image);

	CefRefPtr<CefImage> image_big = CefImage::CreateImage();
	image_big->AddBitmap(1.0, 64, 64, CEF_COLOR_TYPE_RGBA_8888, CEF_ALPHA_TYPE_POSTMULTIPLIED, this->GetIcon64(), 64 * 64 * 4);
	window->SetWindowAppIcon(image_big);

#if defined(CEF_X11)
	// note: the wm_class array includes a '\0' at the end, which should not be included in the actual WM_CLASS
	constexpr char wm_class[] = "BoltLauncher\0BoltLauncher";
	constexpr size_t wm_class_size = sizeof(wm_class) - 1;
	const unsigned long handle = window->GetWindowHandle();
	xcb_change_property(this->xcb, XCB_PROP_MODE_REPLACE, handle, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, wm_class_size, wm_class);
	xcb_flush(this->xcb);
#endif
}

#if defined(BOLT_DEV_LAUNCHER_DIRECTORY)
void Browser::Client::OnFileChange() {
	std::lock_guard<std::mutex> _(this->windows_lock);
	auto it = std::find_if(this->windows.begin(), this->windows.end(), [](CefRefPtr<Window>& win) { return win->IsLauncher(); });
	if (it != this->windows.end()) {
		(*it)->Refresh();
	}
}
#endif

CefRefPtr<CefLifeSpanHandler> Browser::Client::GetLifeSpanHandler() {
	return this;
}

CefRefPtr<CefRequestHandler> Browser::Client::GetRequestHandler() {
	return this;
}

void Browser::Client::OnContextInitialized() {
	fmt::print("[B] OnContextInitialized\n");
	// After main() enters its event loop, this function will be called on the main thread when CEF
	// context is ready to go, so, as suggested by CEF examples, Bolt treats this as an entry point.
#if defined(BOLT_PLUGINS)
	// create an IPC dummy window, in which case OnAfterCreated becomes the normal entry point
	this->ipc_view = CefBrowserView::CreateBrowserView(this, BOLT_IPC_URL, CefBrowserSettings {}, nullptr, nullptr, nullptr);
	CefWindow::CreateTopLevelWindow(this);
#else
	std::lock_guard<std::mutex> _(this->windows_lock);
	CefRefPtr<Browser::Window> w = new Browser::Launcher(this, LAUNCHER_DETAILS, this->show_devtools, this, this->config_dir, this->data_dir);
	this->windows.push_back(w);
#endif
}

bool Browser::Client::OnBeforePopup(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	const CefString& target_url,
	const CefString& target_frame_name,
	CefLifeSpanHandler::WindowOpenDisposition target_disposition,
	bool user_gesture,
	const CefPopupFeatures& popup_features,
	CefWindowInfo& window_info,
	CefRefPtr<CefClient>& client,
	CefBrowserSettings& settings,
	CefRefPtr<CefDictionaryValue>& extra_info,
	bool* no_javascript_access
) {
	std::lock_guard<std::mutex> _(this->windows_lock);
	for (CefRefPtr<Browser::Window>& window: this->windows) {
		window->SetPopupFeaturesForBrowser(browser, popup_features);
	}
	return false;
}

void Browser::Client::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] OnAfterCreated for browser {}\n", browser->GetIdentifier());
#if defined(BOLT_PLUGINS)
	CefRefPtr<CefBrowser> ipc_browser = this->ipc_view->GetBrowser();
	if (ipc_browser && ipc_browser->IsSame(browser)) {
		this->ipc_browser = browser;
		this->windows_lock.lock();
		CefRefPtr<Browser::Window> w = new Browser::Launcher(this, LAUNCHER_DETAILS, this->show_devtools, this, this->config_dir, this->data_dir);
		this->windows.push_back(w);
		this->windows_lock.unlock();
	}
#endif
}

bool Browser::Client::DoClose(CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] DoClose for browser {}\n", browser->GetIdentifier());
	std::lock_guard<std::mutex> _(this->windows_lock);
	this->windows.erase(
		std::remove_if(
			this->windows.begin(),
			this->windows.end(),
			[&browser](const CefRefPtr<Browser::Window>& window){ return window->OnBrowserClosing(browser); }
		),
		this->windows.end()
	);
	return false;
}

void Browser::Client::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] OnBeforeClose for browser {}, remaining windows {}\n", browser->GetIdentifier(), this->windows.size());
#if defined(BOLT_PLUGINS)
	if (this->ipc_browser && this->ipc_browser->IsSame(browser)) {
		this->ipc_browser = nullptr;
		this->ipc_window = nullptr;
		this->ipc_view = nullptr;
		this->Exit();
		return;
	}
#endif
	this->TryExit();
}

bool Browser::Client::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId, CefRefPtr<CefProcessMessage> message) {
	CefString name = message->GetName();

	if (browser->IsSame(this->ipc_browser)) {
		if (name == "__bolt_no_more_clients") {
			fmt::print("[B] no more clients\n");
			this->any_clients = false;
			this->TryExit();
			return true;
		}

		if (name == "__bolt_new_client") {
			fmt::print("[B] new client\n");
			this->any_clients = true;
			return true;
		}

		if (name == "__bolt_open_launcher") {
			fmt::print("[B] open_launcher request\n");
			this->OpenLauncher();
			return true;
		}
	}

	if (name == "__bolt_refresh") {
		this->windows_lock.lock();
		fmt::print("[B] bolt_refresh message received, refreshing browser {}\n", browser->GetIdentifier());
		auto it = std::find_if(
			this->windows.begin(),
			this->windows.end(),
			[&browser](const CefRefPtr<Browser::Window>& window){ return window->HasBrowser(browser); }
		);
		if (it != this->windows.end()) {
			auto browser = *it;
			this->windows_lock.unlock();
			browser->CloseChildrenExceptDevtools();
		}
		browser->ReloadIgnoreCache();
		return true;
	}

	return false;
}

CefRefPtr<CefResourceRequestHandler> Browser::Client::GetResourceRequestHandler(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	bool is_navigation,
	bool is_download,
	const CefString& request_initiator,
	bool& disable_default_handling
) {
	std::lock_guard<std::mutex> _(this->windows_lock);

	// check if this is the IPC dummy window spawning
	if (request->GetURL() == BOLT_IPC_URL) {
		return new Browser::ResourceHandler(nullptr, 0, 204, "text/plain");
	}

	// find the Window responsible for this request, if any
	auto it = std::find_if(
		this->windows.begin(),
		this->windows.end(),
		[&browser](const CefRefPtr<Browser::Window>& window){ return window->HasBrowser(browser); }
	);
	if (it == this->windows.end()) {
		// request came from no window?
		return nullptr;
	} else {
		return (*it)->GetResourceRequestHandler(browser, frame, request, is_navigation, is_download, request_initiator, disable_default_handling);
	}
}

#if defined(BOLT_PLUGINS)
void Browser::Client::IPCHandleNewClient(int fd) {
	fmt::print("[I] new client fd {}\n", fd);
	this->ipc_browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, CefProcessMessage::Create("__bolt_new_client"));

	// immediately send all configured plugins to the new client
	for (const BoltPlugin& plugin: this->plugins) {
		cef_string_utf8_t plugin_name, plugin_desc = {.length = 0}, plugin_path, plugin_main;
		cef_string_to_utf8(plugin.name.c_str(), plugin.name.length(), &plugin_name);
		if (plugin.has_desc) cef_string_to_utf8(plugin.desc.c_str(), plugin.desc.length(), &plugin_desc);
		cef_string_to_utf8(plugin.path.c_str(), plugin.path.length(), &plugin_path);
		cef_string_to_utf8(plugin.main.c_str(), plugin.main.length(), &plugin_main);
		size_t len = sizeof(BoltIPCMessage) + (4 * sizeof(uint32_t)) + plugin_name.length + plugin_desc.length + plugin_path.length + plugin_main.length;
		if (len > 0) {
			char* message = new (std::align_val_t(sizeof(uint32_t))) char[len];
			BoltIPCMessage* header = (BoltIPCMessage*)message;
			header->message_type = IPC_MSG_NEWPLUGINS;
			header->items = 1;
			char* ptr = message + sizeof(BoltIPCMessage);
			memcpy(ptr, &plugin_name.length, sizeof(uint32_t));
			memcpy(ptr + sizeof(uint32_t), plugin_name.str, plugin_name.length);
			ptr += sizeof(uint32_t) + plugin_name.length;
			if (plugin.has_desc) {
				memcpy(ptr, &plugin_desc.length, sizeof(uint32_t));
				memcpy(ptr + sizeof(uint32_t), plugin_desc.str, plugin_desc.length);
				ptr += sizeof(uint32_t) + plugin_desc.length;
			} else {
				const uint32_t no_value = ~0;
				memcpy(ptr, &no_value, sizeof(uint32_t));
				ptr += sizeof(uint32_t);
			}
			memcpy(ptr, &plugin_path.length, sizeof(uint32_t));
			memcpy(ptr + sizeof(uint32_t), plugin_path.str, plugin_path.length);
			ptr += sizeof(uint32_t) + plugin_path.length;
			memcpy(ptr, &plugin_main.length, sizeof(uint32_t));
			memcpy(ptr + sizeof(uint32_t), plugin_main.str, plugin_main.length);
			_bolt_ipc_send(fd, message, len);
			delete[] message;
		}
		cef_string_utf8_clear(&plugin_name);
		if (plugin.has_desc) cef_string_utf8_clear(&plugin_desc);
		cef_string_utf8_clear(&plugin_path);
		cef_string_utf8_clear(&plugin_main);
	}
}

bool Browser::Client::IPCHandleMessage(int fd) {
	BoltIPCMessage message;
	if (_bolt_ipc_receive(fd, &message, sizeof(message))) {
		return false;
	}
	switch (message.message_type) {
		case IPC_MSG_DUPLICATEPROCESS: {
			this->ipc_browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, CefProcessMessage::Create("__bolt_open_launcher"));
			break;
		}
		default: {
			fmt::print("[I] got unknown message type {}\n", message.message_type);
			break;
		}
	}
	return true;
}

void Browser::Client::IPCHandleNoMoreClients() {
	this->ipc_browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, CefProcessMessage::Create("__bolt_no_more_clients"));
}

void Browser::Client::OnWindowCreated(CefRefPtr<CefWindow> window) {
	// used only for dummy IPC window; real browsers have their own OnWindowCreated override
	this->ipc_window = window;
	window->AddChildView(this->ipc_view);
}
#endif
