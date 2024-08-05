#include "client.hxx"

#if defined(BOLT_PLUGINS)
#include "include/cef_parser.h"
#include <cstring>
#define BOLT_IPC_URL "https://bolt-blankpage/"
#endif

#include "include/cef_life_span_handler.h"
#include "include/cef_app.h"
#include "include/cef_values.h"
#include "include/internal/cef_types.h"
#include "resource_handler.hxx"
#include "window_launcher.hxx"

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

Browser::Client::Client(CefRefPtr<Browser::App> app,std::filesystem::path config_dir, std::filesystem::path data_dir, std::filesystem::path runtime_dir):
#if defined(BOLT_DEV_LAUNCHER_DIRECTORY)
	CLIENT_FILEHANDLER(BOLT_DEV_LAUNCHER_DIRECTORY),
#endif
#if defined(BOLT_PLUGINS)
	next_client_uid(0),
#endif
	show_devtools(SHOW_DEVTOOLS), config_dir(config_dir), data_dir(data_dir), runtime_dir(runtime_dir)
{
	app->SetBrowserProcessHandler(this);

#if defined(BOLT_PLUGINS)
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
	this->game_clients_lock.lock();
	size_t client_count = this->game_clients.size();
	this->game_clients_lock.unlock();

	bool can_exit = (window_count == 0) && (client_count == 0);
	fmt::print("[B] TryExit, windows={}, clients={}, exit={}\n", window_count, client_count, can_exit);
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

#if defined(BOLT_PLUGINS)
	if (browser->IsSame(this->ipc_browser)) {
		if (name == "__bolt_no_more_clients") {
			fmt::print("[B] no more clients\n");
			this->TryExit();
			return true;
		}

		if (name == "__bolt_open_launcher") {
			fmt::print("[B] open_launcher request\n");
			this->OpenLauncher();
			return true;
		}
	}
#endif

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

#if defined(BOLT_PLUGINS)
	// check if this is the IPC dummy window spawning
	if (request->GetURL() == BOLT_IPC_URL) {
		return new Browser::ResourceHandler(nullptr, 0, 204, "text/plain");
	}
#endif

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
	this->game_clients_lock.lock();
	this->game_clients.push_back(GameClient {.uid = this->next_client_uid, .fd = fd, .identity = nullptr});
	this->next_client_uid += 1;
	this->game_clients_lock.unlock();
}

void Browser::Client::IPCHandleClientListUpdate() {
	std::lock_guard<std::mutex> _(this->windows_lock);
	auto it = std::find_if(this->windows.begin(), this->windows.end(), [](CefRefPtr<Window>& win) { return win->IsLauncher(); });
	if (it != this->windows.end()) {
		(*it)->SendMessage("__bolt_client_list_update");
	}
}

void Browser::Client::IPCHandleClosed(int fd) {
	std::lock_guard<std::mutex> _(this->game_clients_lock);
	auto it = std::find_if(this->game_clients.begin(), this->game_clients.end(), [fd](const GameClient& g) { return g.fd == fd; });
	if (it != this->game_clients.end()) {
		delete[] it->identity;
		this->game_clients.erase(it);
	}
}

bool Browser::Client::IPCHandleMessage(int fd) {
	BoltIPCMessageToHost message;
	if (_bolt_ipc_receive(fd, &message, sizeof(message))) {
		return false;
	}
	switch (message.message_type) {
		case IPC_MSG_DUPLICATEPROCESS: {
			this->ipc_browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, CefProcessMessage::Create("__bolt_open_launcher"));
			break;
		}
		case IPC_MSG_IDENTIFY: {
			this->game_clients_lock.lock();
			for (GameClient& g: this->game_clients) {
				if (g.fd == fd) {
					g.identity = new char[message.items + 1];
					_bolt_ipc_receive(fd, g.identity, message.items);
					g.identity[message.items] = '\0';
					break;
				}
			}
			this->game_clients_lock.unlock();
			this->IPCHandleClientListUpdate();
			break;
		}
		default: {
			fmt::print("[I] got unknown message type {}\n", (int)message.message_type);
			break;
		}
	}
	return true;
}

void Browser::Client::IPCHandleNoMoreClients() {
	this->ipc_browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, CefProcessMessage::Create("__bolt_no_more_clients"));
}

CefRefPtr<CefResourceRequestHandler> Browser::Client::ListGameClients() {
	char buf[256];
	CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
	this->game_clients_lock.lock();
	for (const GameClient& g: this->game_clients) {
		CefRefPtr<CefDictionaryValue> inner_dict = CefDictionaryValue::Create();
		snprintf(buf, 256, "%llu", (unsigned long long)g.uid);
		if (g.identity) {
			inner_dict->SetString("identity", g.identity);
		}
		dict->SetDictionary(buf, inner_dict);
	}
	this->game_clients_lock.unlock();
	CefRefPtr<CefValue> value = CefValue::Create();
	value->SetDictionary(dict);
	std::string str = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString();
	return new ResourceHandler(std::move(str), 200, "application/json");
}

void Browser::Client::StartPlugin(uint64_t client_id, std::string id, std::string path, std::string main) {
	this->game_clients_lock.lock();
	for (const GameClient& g: this->game_clients) {
		if (g.uid == client_id) {
			const size_t message_size = sizeof(BoltIPCMessageToClient) + (sizeof(uint32_t) * 3) + id.size() + path.size() + main.size();
			uint8_t* message = new uint8_t[message_size];
			*(BoltIPCMessageToClient*)message = {.message_type = IPC_MSG_STARTPLUGINS, .items = 1};
			size_t pos = sizeof(BoltIPCMessageToClient);
			*(uint32_t*)(message + pos) = id.size();
			pos += sizeof(uint32_t);
			*(uint32_t*)(message + pos) = path.size();
			pos += sizeof(uint32_t);
			*(uint32_t*)(message + pos) = main.size();
			pos += sizeof(uint32_t);
			memcpy(message + pos, id.data(), id.size());
			pos += id.size();
			memcpy(message + pos, path.data(), path.size());
			pos += path.size();
			memcpy(message + pos, main.data(), main.size());
			_bolt_ipc_send(g.fd, message, message_size);
			delete[] message;
			break;
		}
	}
	this->game_clients_lock.unlock();
}

void Browser::Client::OnWindowCreated(CefRefPtr<CefWindow> window) {
	// used only for dummy IPC window; real browsers have their own OnWindowCreated override
	this->ipc_window = window;
	window->AddChildView(this->ipc_view);
}
#endif
