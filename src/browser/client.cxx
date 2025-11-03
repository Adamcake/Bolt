#include "client.hxx"
#include "include/internal/cef_string.h"
#include <filesystem>

#if defined(BOLT_PLUGINS)
#include "window_osr.hxx"
#include "window_plugin.hxx"
#include "../library/event.h"
#include <cstring>
#endif

#include "include/cef_app.h"
#include "include/cef_values.h"
#include "include/internal/cef_types.h"
#include "resource_handler.hxx"
#include "window_launcher.hxx"

#include <algorithm>
#include <fcntl.h>
#include <fmt/core.h>

#if defined(BOLT_DEV_SHOW_DEVTOOLS)
constexpr bool SHOW_DEVTOOLS = true;
#else
constexpr bool SHOW_DEVTOOLS = false;
#endif

struct TryExitTask: public CefTask {
	TryExitTask(CefRefPtr<Browser::Client> client): client(client) {}
	void Execute() override { client->TryExit(true); }
	private:
		CefRefPtr<Browser::Client> client;
		IMPLEMENT_REFCOUNTING(TryExitTask);
		DISALLOW_COPY_AND_ASSIGN(TryExitTask);
};

struct OpenLauncherTask: public CefTask {
	OpenLauncherTask(CefRefPtr<Browser::Client> client): client(client) {}
	void Execute() override { client->OpenLauncher(); }
	private:
		CefRefPtr<Browser::Client> client;
		IMPLEMENT_REFCOUNTING(OpenLauncherTask);
		DISALLOW_COPY_AND_ASSIGN(OpenLauncherTask);
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
}

void Browser::Client::OpenLauncher() {
	fmt::print("[B] OpenLauncher\n");
	std::lock_guard<std::mutex> _(this->launcher_lock);
	if (this->launcher) {
		this->launcher->Focus();
	} else {
		this->launcher = new Browser::Launcher(this, this->LauncherDetails(), this->show_devtools, this, this->config_dir, this->data_dir);
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
		this->Exit();
	}
}

void Browser::Client::Exit() {
	this->StopFileManager();
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

void Browser::Client::OnContextInitialized() {
	fmt::print("[B] OnContextInitialized\n");
	// After main() enters its event loop, this function will be called on the main thread when CEF
	// context is ready to go, so, as suggested by CEF examples, Bolt treats this as an entry point.
	this->launcher_lock.lock();
	this->launcher = new Browser::Launcher(this, this->LauncherDetails(), this->show_devtools, this, this->config_dir, this->data_dir);
	this->launcher_lock.unlock();
}

Browser::Details Browser::Client::LauncherDetails() {
	int startx = 0;
	int starty = 0;
	std::filesystem::path p = this->config_dir;
	p.append(LAUNCHER_BIN_FILENAME);
	std::ifstream f(p, std::ios::binary | std::ios::in);
	if (!f.fail()) {
		int numbers[3];
		uint8_t buf[2];
		bool success = true;
		for (size_t i = 0; i < sizeof(numbers) / sizeof(*numbers); i += 1) {
			f.read(reinterpret_cast<char*>(buf), sizeof(buf));
			if (f.fail() || f.eof()) {
				success = false;
				break;
			}
			numbers[i] = static_cast<int16_t>(static_cast<uint16_t>(buf[0]) + (static_cast<uint16_t>(buf[1]) << 8));
		}
		if (success && numbers[0] == 1) {
			startx = numbers[1];
			starty = numbers[2];
		}
		f.close();
	}

	return Details {
		.preferred_width = 800,
		.preferred_height = 608,
		.startx = startx,
		.starty = starty,
		.center_on_open = true,
		.resizeable = true,
		.frame = true,
		.is_devtools = false,
		.has_custom_js = false,
	};
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
		CefPostTask(TID_UI, new OpenLauncherTask(this));
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

			char* custom_js = nullptr;
			if (header.has_custom_js) {
				custom_js = new char[header.custom_js_length];
				_bolt_ipc_receive(fd, custom_js, header.custom_js_length);
			}

			CefRefPtr<ActivePlugin> plugin = this->GetPluginFromFDAndID(client, header.plugin_id);
			Browser::Details details {
				.preferred_width = header.w,
				.preferred_height = header.h,
				.center_on_open = true,
				.resizeable = true,
				.frame = true,
				.has_custom_js = true,
				.custom_js = CefString(custom_js, header.custom_js_length),
			};
			plugin->windows.push_back(new Browser::PluginWindow(this, details, url, plugin, fd, &this->send_lock, header.window_id, header.plugin_id, false));
			delete[] url;
			delete[] custom_js;
			break;
		}
		case IPC_MSG_CREATEBROWSER_OSR: {
			BoltIPCCreateBrowserHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			char* url = new char[header.url_length + 1];
			_bolt_ipc_receive(fd, url, header.url_length);
			url[header.url_length] = '\0';

			char* custom_js = nullptr;
			if (header.has_custom_js) {
				custom_js = new char[header.custom_js_length];
				_bolt_ipc_receive(fd, custom_js, header.custom_js_length);
			}

			CefRefPtr<ActivePlugin> plugin = this->GetPluginFromFDAndID(client, header.plugin_id);
			if (plugin) {
				CefRefPtr<Browser::WindowOSR> window = new Browser::WindowOSR(CefString((char*)url), header.w, header.h, fd, this, &this->send_lock, header.pid, header.window_id, header.plugin_id, plugin, custom_js);
				plugin->windows_osr.push_back(window);
			}
			delete[] url;
			delete[] custom_js;
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
		case IPC_MSG_SHOWDEVTOOLS_EXTERNAL: {
			BoltIPCShowDevtoolsHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			CefRefPtr<Browser::PluginWindow> window = this->GetExternalWindowFromFDAndIDs(client, header.plugin_id, header.window_id);
			if (window && !window->IsDeleted()) window->HandleShowDevtools();
			break;
		}
		case IPC_MSG_SHOWDEVTOOLS_OSR: {
			BoltIPCShowDevtoolsHeader header;
			_bolt_ipc_receive(fd, &header, sizeof(header));
			CefRefPtr<Browser::WindowOSR> window = this->GetOsrWindowFromFDAndIDs(client, header.plugin_id, header.window_id);
			if (window && !window->IsDeleted()) window->HandleShowDevtools();
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
	fmt::print("[B] no more clients\n");
	CefPostTask(TID_UI, new TryExitTask(this));
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

			std::filesystem::path config_path = this->config_dir;
			config_path.append("plugins");
			config_path.append(id);
			std::error_code err;
			std::filesystem::create_directories(config_path, err);
			const std::string config = config_path.string() + '/';
			if (err.value()) {
				fmt::print("[B] couldn't start plugin: failed to create plugin config directory '{}'\n", config);
				break;
			}

			g.plugins.push_back(new ActivePlugin(this->next_plugin_uid, id, path, false));

			const BoltIPCMessageTypeToClient msg_type = IPC_MSG_STARTPLUGIN;
			const BoltIPCStartPluginHeader header = {
				.uid = this->next_plugin_uid,
				.path_size = static_cast<uint32_t>(path.size()),
				.main_size = static_cast<uint32_t>(main.size()),
				.config_path_size = static_cast<uint32_t>(config.size()),
			};
			this->send_lock.lock();
			_bolt_ipc_send(g.fd, &msg_type, sizeof(msg_type));
			_bolt_ipc_send(g.fd, &header, sizeof(header));
			_bolt_ipc_send(g.fd, path.data(), path.size());
			_bolt_ipc_send(g.fd, main.data(), main.size());
			_bolt_ipc_send(g.fd, config.data(), config.size());
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

Browser::Client::ActivePlugin::ActivePlugin(uint64_t uid, std::string id, std::filesystem::path path, bool watch): Directory(path, watch), uid(uid), id(id), deleted(false) { }

#endif
