#include "client.hxx"
#include "include/internal/cef_string.h"

#if defined(BOLT_PLUGINS)
#include "window_osr.hxx"
#include "window_plugin.hxx"
#include "../library/event.h"
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
	CLIENT_FILEHANDLER(BOLT_DEV_LAUNCHER_DIRECTORY, true),
#endif
#if defined(BOLT_PLUGINS)
	next_client_uid(0), next_plugin_uid(0),
#endif
	show_devtools(SHOW_DEVTOOLS), config_dir(config_dir), data_dir(data_dir), runtime_dir(runtime_dir), launcher(nullptr)
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
	std::lock_guard<std::mutex> _(this->launcher_lock);
	if (this->launcher) {
		this->launcher->Focus();
	} else {
		this->launcher = new Browser::Launcher(this, LAUNCHER_DETAILS, this->show_devtools, this, this->config_dir, this->data_dir);
	}
}

void Browser::Client::OnLauncherClosed() {
	this->launcher_lock.lock();
	this->launcher = nullptr;
	this->launcher_lock.unlock();
	this->TryExit(false);
}

void Browser::Client::TryExit(bool check_launcher) {
	bool launcher_open = check_launcher;
	if (check_launcher) {
		this->launcher_lock.lock();
		launcher_open = this->launcher != nullptr;
		this->launcher_lock.unlock();
	}

#if defined(BOLT_PLUGINS)
	this->game_clients_lock.lock();
	size_t client_count = this->game_clients.size();
	this->game_clients_lock.unlock();

	bool can_exit = !launcher_open && (client_count == 0);
	fmt::print("[B] TryExit, launcher={}, clients={}, exit={}\n", launcher_open, client_count, can_exit);
#else
	bool can_exit = !launcher_open;
	fmt::print("[B] TryExit, launcher={}, exit={}\n", launcher_open, can_exit);
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
	this->launcher_lock.lock();
	if (this->launcher) {
		this->launcher->Refresh();
	}
	this->launcher_lock.unlock();
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
	this->launcher_lock.lock();
	this->launcher = new Browser::Launcher(this, LAUNCHER_DETAILS, this->show_devtools, this, this->config_dir, this->data_dir);
	this->launcher_lock.unlock();
#endif
}

void Browser::Client::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
	// usually only called for IPC dummy window
	fmt::print("[B] Client::OnAfterCreated for browser {}\n", browser->GetIdentifier());
#if defined(BOLT_PLUGINS)
	CefRefPtr<CefBrowser> ipc_browser = this->ipc_view->GetBrowser();
	if (ipc_browser && ipc_browser->IsSame(browser)) {
		this->ipc_browser = browser;
		this->launcher_lock.lock();
		this->launcher = new Browser::Launcher(this, LAUNCHER_DETAILS, this->show_devtools, this, this->config_dir, this->data_dir);
		this->launcher_lock.unlock();
	}
#endif
}

void Browser::Client::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	// usually only called for IPC dummy window
	fmt::print("[B] Client::OnBeforeClose for browser {}\n", browser->GetIdentifier());
#if defined(BOLT_PLUGINS)
	if (this->ipc_browser && this->ipc_browser->IsSame(browser)) {
		this->ipc_browser = nullptr;
		this->ipc_window = nullptr;
		this->ipc_view = nullptr;
		this->Exit();
		return;
	}
#endif
}

bool Browser::Client::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId, CefRefPtr<CefProcessMessage> message) {
	CefString name = message->GetName();

#if defined(BOLT_PLUGINS)
	if (browser->IsSame(this->ipc_browser)) {
		if (name == "__bolt_no_more_clients") {
			fmt::print("[B] no more clients\n");
			this->TryExit(true);
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
		this->launcher_lock.lock();
		fmt::print("[B] bolt_refresh message received, refreshing browser {}\n", browser->GetIdentifier());
		if (this->launcher) {
			this->launcher->CloseChildrenExceptDevtools();
		}
		this->launcher_lock.unlock();
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
#if defined(BOLT_PLUGINS)
	// check if this is the IPC dummy window spawning
	if (request->GetURL() == BOLT_IPC_URL) {
		return new Browser::ResourceHandler(nullptr, 0, 204, "text/plain");
	}
#endif

	CefRefPtr<CefResourceRequestHandler> ret = nullptr;
	this->launcher_lock.lock();
	if (this->launcher) {
		ret = this->launcher->GetResourceRequestHandler(browser, frame, request, is_navigation, is_download, request_initiator, disable_default_handling);
	}
	this->launcher_lock.unlock();
	return ret;
}

#if defined(BOLT_PLUGINS)
void Browser::Client::IPCHandleNewClient(int fd) {
	fmt::print("[I] new client fd {}\n", fd);
	this->game_clients_lock.lock();
	this->game_clients.push_back(GameClient {.uid = this->next_client_uid, .fd = fd, .deleted = false, .identity = nullptr});
	this->next_client_uid += 1;
	this->game_clients_lock.unlock();
}

void Browser::Client::IPCHandleClientListUpdate(bool need_lock_mutex) {
	this->launcher_lock.lock();
	if (this->launcher) {
		this->launcher->UpdateClientList(need_lock_mutex);
	}
	this->launcher_lock.unlock();
}

void Browser::Client::IPCHandleClosed(int fd) {
	std::lock_guard<std::mutex> _(this->game_clients_lock);
	auto it = std::find_if(this->game_clients.begin(), this->game_clients.end(), [fd](const GameClient& g) { return g.fd == fd; });
	if (it != this->game_clients.end()) {
		bool delete_now = true;
		delete[] it->identity;
		it->deleted = true;
		for (CefRefPtr<ActivePlugin>& p: it->plugins) {
			p->deleted = true;
			for (CefRefPtr<WindowOSR>& w: p->windows_osr) {
				if (!w->IsDeleted()) w->Close();
				delete_now = false;
			}
			for (CefRefPtr<PluginWindow>& w: p->windows) {
				if (!w->IsDeleted()) w->Close();
				delete_now = false;
			}
		}
		if (delete_now) {
			this->game_clients.erase(it);
		}
	}
}

// convenience function, assumes the mutex is already locked
CefRefPtr<Browser::PluginWindow> Browser::Client::GetExternalWindowFromFDAndIDs(GameClient* client, uint64_t plugin_id, uint64_t window_id) {
	for (CefRefPtr<ActivePlugin>& p: client->plugins) {
		if (p->deleted || p->uid != plugin_id) continue;
		for (CefRefPtr<Browser::PluginWindow>& w: p->windows) {
			if (!w->IsDeleted() && w->WindowID() == window_id) {
				return w;
			}
		}
	}
	return nullptr;
}

// convenience function, assumes the mutex is already locked
CefRefPtr<Browser::WindowOSR> Browser::Client::GetOsrWindowFromFDAndIDs(GameClient* client, uint64_t plugin_id, uint64_t window_id) {
	for (CefRefPtr<ActivePlugin>& p: client->plugins) {
		if (p->deleted || p->uid != plugin_id) continue;
		for (CefRefPtr<Browser::WindowOSR>& w: p->windows_osr) {
			if (!w->IsDeleted() && w->WindowID() == window_id) {
				return w;
			}
		}
	}
	return nullptr;
}

// convenience function, assumes the mutex is already locked
CefRefPtr<Browser::Client::ActivePlugin> Browser::Client::GetPluginFromFDAndID(GameClient* client, uint64_t id) {
	for (CefRefPtr<ActivePlugin>& p: client->plugins) {
		if (p->deleted) continue;
		if (p->uid == id) return p;
	}
	return nullptr;
}

bool Browser::Client::IPCHandleMessage(int fd) {
	BoltIPCMessageTypeToHost msg_type;
	if (_bolt_ipc_receive(fd, &msg_type, sizeof(msg_type))) {
		return false;
	}

	if (msg_type == IPC_MSG_DUPLICATEPROCESS) {
		this->ipc_browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, CefProcessMessage::Create("__bolt_open_launcher"));
		return true;
	}

	this->game_clients_lock.lock();
	const auto it = std::find_if(this->game_clients.begin(), this->game_clients.end(), [fd](const GameClient& g){ return !g.deleted && g.fd == fd; });
	if (it == this->game_clients.end()) {
		this->game_clients_lock.unlock();
		return true;
	}
	GameClient* const client = &(*it);

	switch (msg_type) {
		case IPC_MSG_IDENTIFY: {
			struct BoltIPCIdentifyHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			delete[] client->identity;
			client->identity = new char[header.name_length + 1];
			_bolt_ipc_receive(fd, client->identity, header.name_length);
			client->identity[header.name_length] = '\0';
			this->IPCHandleClientListUpdate(false);
			break;
		}
		case IPC_MSG_CLIENT_STOPPED_PLUGIN: {
			BoltIPCClientStoppedPluginHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			for (auto it = client->plugins.begin(); it != client->plugins.end(); it++) {
				if ((*it)->deleted || (*it)->uid != header.plugin_id) continue;
				(*it)->deleted = true;
				for (CefRefPtr<WindowOSR>& w: (*it)->windows_osr) {
					if (!w->IsDeleted()) w->Close();
				}
				for (CefRefPtr<PluginWindow>& w: (*it)->windows) {
					if (!w->IsDeleted()) w->Close();
				}
				if ((*it)->windows.empty() && (*it)->windows_osr.empty()) {
					client->plugins.erase(it);
				}
				break;
			}
			this->IPCHandleClientListUpdate(false);
			break;
		}
		case IPC_MSG_CREATEBROWSER_EXTERNAL: {
			BoltIPCCreateBrowserHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			char* url = new char[header.url_length + 1];
			_bolt_ipc_receive(fd, url, header.url_length);
			url[header.url_length] = '\0';

			CefRefPtr<ActivePlugin> plugin = this->GetPluginFromFDAndID(client, header.plugin_id);
			Browser::Details details {
				.preferred_width = header.w,
				.preferred_height = header.h,
				.center_on_open = true,
				.resizeable = true,
				.frame = true,
			};
			plugin->windows.push_back(new Browser::PluginWindow(this, details, url, plugin, fd, &this->send_lock, header.window_id, header.plugin_id, false));
			delete[] url;
			break;
		}
		case IPC_MSG_CREATEBROWSER_OSR: {
			BoltIPCCreateBrowserHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			char* url = new char[header.url_length + 1];
			_bolt_ipc_receive(fd, url, header.url_length);
			url[header.url_length] = '\0';

			CefRefPtr<ActivePlugin> plugin = this->GetPluginFromFDAndID(client, header.plugin_id);
			if (plugin) {
				CefRefPtr<Browser::WindowOSR> window = new Browser::WindowOSR(CefString((char*)url), header.w, header.h, fd, this, &this->send_lock, header.pid, header.window_id, header.plugin_id, plugin);
				plugin->windows_osr.push_back(window);
			}
			delete[] url;
			break;
		}
		case IPC_MSG_CLOSEBROWSER_EXTERNAL: {
			BoltIPCCloseBrowserHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			CefRefPtr<Browser::PluginWindow> window = this->GetExternalWindowFromFDAndIDs(client, header.plugin_id, header.window_id);
			if (window && !window->IsDeleted()) window->Close();
			break;
		}
		case IPC_MSG_CLOSEBROWSER_OSR: {
			BoltIPCCloseBrowserHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			CefRefPtr<Browser::WindowOSR> window = this->GetOsrWindowFromFDAndIDs(client, header.plugin_id, header.window_id);
			if (window && !window->IsDeleted()) window->Close();
			break;
		}
		case IPC_MSG_OSRUPDATE_ACK: {
			BoltIPCOsrUpdateAckHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			CefRefPtr<Browser::WindowOSR> window = this->GetOsrWindowFromFDAndIDs(client, header.plugin_id, header.window_id);
			if (window) window->HandleAck();
			break;
		}
		case IPC_MSG_CAPTURENOTIFY_EXTERNAL: {
			BoltIPCCaptureNotifyHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			CefRefPtr<Browser::PluginWindow> window = this->GetExternalWindowFromFDAndIDs(client, header.plugin_id, header.window_id);
			if (window && !window->IsDeleted()) window->HandleCaptureNotify(header.pid, header.capture_id, header.width, header.height, header.needs_remap != 0);
			break;
		}
		case IPC_MSG_CAPTURENOTIFY_OSR: {
			BoltIPCCaptureNotifyHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			CefRefPtr<Browser::WindowOSR> window = this->GetOsrWindowFromFDAndIDs(client, header.plugin_id, header.window_id);
			if (window && !window->IsDeleted()) window->HandleCaptureNotify(header.pid, header.capture_id, header.width, header.height, header.needs_remap != 0);
			break;
		}

#define DEF_OSR_EVENT(EVNAME, HANDLER, EVTYPE) case IPC_MSG_EV##EVNAME: { \
	BoltIPCEvHeader header; \
	EVTYPE event; \
	_bolt_ipc_receive(fd, &header, sizeof(header)); \
	_bolt_ipc_receive(fd, &event, sizeof(event)); \
	CefRefPtr<Browser::WindowOSR> window = this->GetOsrWindowFromFDAndIDs(client, header.plugin_id, header.window_id); \
	if (window) window->HANDLER(&event); \
	break; \
}
		DEF_OSR_EVENT(REPOSITION, HandleReposition, RepositionEvent)
		DEF_OSR_EVENT(MOUSEMOTION, HandleMouseMotion, MouseMotionEvent)
		DEF_OSR_EVENT(MOUSEBUTTON, HandleMouseButton, MouseButtonEvent)
		DEF_OSR_EVENT(MOUSEBUTTONUP, HandleMouseButtonUp, MouseButtonEvent)
		DEF_OSR_EVENT(SCROLL, HandleScroll, MouseScrollEvent)
		DEF_OSR_EVENT(MOUSELEAVE, HandleMouseLeave, MouseMotionEvent)
#undef DEF_OSR_EVENT

		case IPC_MSG_PLUGINMESSAGE: {
			BoltIPCPluginMessageHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			uint8_t* content = new uint8_t[header.message_size];
			_bolt_ipc_receive(fd, content, header.message_size);
			CefRefPtr<Browser::PluginWindow> window = this->GetExternalWindowFromFDAndIDs(client, header.plugin_id, header.window_id);
			if (window) window->HandlePluginMessage(content, header.message_size);
			delete[] content;
			break;
		}
		case IPC_MSG_OSRPLUGINMESSAGE: {
			BoltIPCPluginMessageHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			uint8_t* content = new uint8_t[header.message_size];
			_bolt_ipc_receive(fd, content, header.message_size);
			CefRefPtr<Browser::WindowOSR> window = this->GetOsrWindowFromFDAndIDs(client, header.plugin_id, header.window_id);
			if (window) window->HandlePluginMessage(content, header.message_size);
			delete[] content;
			break;
		}
		default:
			fmt::print("[I] got unknown message type {}\n", static_cast<int>(msg_type));
			break;
	}
	this->game_clients_lock.unlock();
	return true;
}

void Browser::Client::IPCHandleNoMoreClients() {
	this->ipc_browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, CefProcessMessage::Create("__bolt_no_more_clients"));
}

void Browser::Client::ListGameClients(CefRefPtr<CefListValue> list, bool need_lock_mutex) {
	if (need_lock_mutex) this->game_clients_lock.lock();
	list->SetSize(std::count_if(this->game_clients.begin(), this->game_clients.end(), [](const GameClient& c){ return !c.deleted; }));
	size_t list_index = 0;
	for (const GameClient& g: this->game_clients) {
		if (g.deleted) continue;
		CefRefPtr<CefDictionaryValue> inner_dict = CefDictionaryValue::Create();
		if (g.identity) inner_dict->SetString("identity", g.identity);
		inner_dict->SetInt("uid", g.uid);
		CefRefPtr<CefListValue> plugin_list = CefListValue::Create();
		plugin_list->SetSize(std::count_if(g.plugins.begin(), g.plugins.end(), [](const CefRefPtr<ActivePlugin> p){ return !p->deleted; }));
		size_t plugin_list_index = 0;
		for (const CefRefPtr<ActivePlugin>& p: g.plugins) {
			if (p->deleted) continue;
			CefRefPtr<CefDictionaryValue> plugin_dict = CefDictionaryValue::Create();
			plugin_dict->SetInt("uid", p->uid);
			plugin_dict->SetString("id", CefString(p->id));
			plugin_list->SetDictionary(plugin_list_index, plugin_dict);
			plugin_list_index += 1;
		}
		inner_dict->SetList("plugins", plugin_list);
		list->SetDictionary(list_index, inner_dict);
		list_index += 1;
	}
	if (need_lock_mutex) this->game_clients_lock.unlock();
}

void Browser::Client::StartPlugin(uint64_t client_id, std::string id, std::string path, std::string main) {
	this->game_clients_lock.lock();
	for (GameClient& g: this->game_clients) {
		if (!g.deleted && g.uid == client_id) {
			for (auto it = g.plugins.begin(); it != g.plugins.end(); it++) {
				if ((*it)->id != id) continue;
				const BoltIPCMessageTypeToClient msg_type = IPC_MSG_HOST_STOPPED_PLUGIN;
				const BoltIPCHostStoppedPluginHeader header = { .plugin_id = (*it)->uid };
				this->send_lock.lock();
				_bolt_ipc_send(g.fd, &msg_type, sizeof(msg_type));
				_bolt_ipc_send(g.fd, &header, sizeof(header));
				this->send_lock.unlock();
				(*it)->deleted = true;
				for (CefRefPtr<WindowOSR>& w: (*it)->windows_osr) {
					w->Close();
				}
				for (CefRefPtr<PluginWindow>& w: (*it)->windows) {
					w->Close();
				}
				if ((*it)->windows.empty() && (*it)->windows_osr.empty()) {
					g.plugins.erase(it);
				}
				break;
			}
			g.plugins.push_back(new ActivePlugin(this->next_plugin_uid, id, path, false));

			const BoltIPCMessageTypeToClient msg_type = IPC_MSG_STARTPLUGIN;
			const BoltIPCStartPluginHeader header = { .uid = this->next_plugin_uid, .path_size = static_cast<uint32_t>(path.size()), .main_size = static_cast<uint32_t>(main.size()) };
			this->send_lock.lock();
			_bolt_ipc_send(g.fd, &msg_type, sizeof(msg_type));
			_bolt_ipc_send(g.fd, &header, sizeof(header));
			_bolt_ipc_send(g.fd, path.data(), path.size());
			_bolt_ipc_send(g.fd, main.data(), main.size());
			this->send_lock.unlock();
			this->next_plugin_uid += 1;
			break;
		}
	}
	this->game_clients_lock.unlock();
}

void Browser::Client::StopPlugin(uint64_t client_id, uint64_t uid) {
	this->game_clients_lock.lock();
	for (GameClient& g: this->game_clients) {
		if (!g.deleted && g.uid == client_id) {
			for (auto it = g.plugins.begin(); it != g.plugins.end(); it++) {
				if ((*it)->uid != uid) continue;
				const BoltIPCMessageTypeToClient msg_type = IPC_MSG_HOST_STOPPED_PLUGIN;
				const BoltIPCHostStoppedPluginHeader header = { .plugin_id = (*it)->uid };
				this->send_lock.lock();
				_bolt_ipc_send(g.fd, &msg_type, sizeof(msg_type));
				_bolt_ipc_send(g.fd, &header, sizeof(header));
				this->send_lock.unlock();
				(*it)->deleted = true;
				for (CefRefPtr<PluginWindow>& w: (*it)->windows) {
					w->Close();
				}
				for (CefRefPtr<WindowOSR>& w: (*it)->windows_osr) {
					w->Close();
				}
				if ((*it)->windows.empty() && (*it)->windows_osr.empty()) {
					g.plugins.erase(it);
				}
				break;
			}
		}
	}
	this->game_clients_lock.unlock();
}

void Browser::Client::CleanupClientPlugins(int fd) {
	this->game_clients_lock.lock();
	for (auto it = this->game_clients.begin(); it != this->game_clients.end(); it += 1) {
		if (it->fd != fd) continue;
		for (CefRefPtr<ActivePlugin>& p: it->plugins) {
			p->windows_osr.erase(
				std::remove_if(
					p->windows_osr.begin(),
					p->windows_osr.end(),
					[](const CefRefPtr<WindowOSR>& window){ return window->IsDeleted(); }
				),
				p->windows_osr.end()
			);
			p->windows.erase(
				std::remove_if(
					p->windows.begin(),
					p->windows.end(),
					[](const CefRefPtr<PluginWindow>& window){ return window->IsDeleted(); }
				),
				p->windows.end()
			);
		}
		it->plugins.erase(
			std::remove_if(
				it->plugins.begin(),
				it->plugins.end(),
				[](const CefRefPtr<ActivePlugin>& plugin) {
					return plugin->deleted && plugin->windows_osr.size() == 0 && plugin->windows.size() == 0;
				}
			),
			it->plugins.end()
		);
		if (it->deleted && it->plugins.size() == 0) {
			this->game_clients.erase(it);
		}
		break;
	}
	this->game_clients_lock.unlock();
}

void Browser::Client::OnWindowCreated(CefRefPtr<CefWindow> window) {
	// used only for dummy IPC window; real browsers have their own OnWindowCreated override
	this->ipc_window = window;
	window->AddChildView(this->ipc_view);
}

Browser::Client::ActivePlugin::ActivePlugin(uint64_t uid, std::string id, std::filesystem::path path, bool watch): Directory(path, watch), uid(uid), id(id), deleted(false) { }

#endif
