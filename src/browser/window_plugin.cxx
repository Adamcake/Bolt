#include "window_plugin.hxx"

#if defined(BOLT_PLUGINS)
#include "client.hxx"
#include "include/cef_base.h"
#include <fmt/core.h>

struct InitTask: public CefTask {
	InitTask(CefRefPtr<Browser::PluginWindow> window, const char* url): window(window), url(CefString(url)) {}
	void Execute() override {
		this->window->Init(this->url);
	}
	private:
		CefRefPtr<Browser::PluginWindow> window;
		CefString url;
		IMPLEMENT_REFCOUNTING(InitTask);
		DISALLOW_COPY_AND_ASSIGN(InitTask);
};

Browser::PluginWindow::PluginWindow(CefRefPtr<Client> main_client, Details details, const char* url, CefRefPtr<FileManager::Directory> file_manager, BoltSocketType fd, std::mutex* send_lock, uint64_t id, uint64_t plugin_id, bool show_devtools):
	PluginRequestHandler(IPC_MSG_EXTERNALBROWSERMESSAGE, send_lock),
	Window(main_client, details, show_devtools),
	file_manager(file_manager), client_fd(fd), window_id(id), plugin_id(plugin_id), closing(false), deleted(false)
{
	if (CefCurrentlyOn(TID_UI)) {
		this->Init(CefString(url));
	} else {
		CefPostTask(TID_UI, new InitTask(this, url));
	}
}

bool Browser::PluginWindow::IsDeleted() const {
	return this->deleted;
}

uint64_t Browser::PluginWindow::WindowID() const {
	return this->window_id;
}

uint64_t Browser::PluginWindow::PluginID() const {
	return this->plugin_id;
}

BoltSocketType Browser::PluginWindow::ClientFD() const {
	return this->client_fd;
}

CefRefPtr<FileManager::FileManager> Browser::PluginWindow::FileManager() const {
	return this->file_manager;
}

CefRefPtr<CefBrowser> Browser::PluginWindow::Browser() const {
	return this->browser;
}

void Browser::PluginWindow::HandlePluginCloseRequest() {
	const BoltIPCMessageTypeToClient msg_type = IPC_MSG_BROWSERCLOSEREQUEST;
	const BoltIPCBrowserCloseRequestHeader header = { .window_id = this->window_id, .plugin_id = this->plugin_id };
	this->send_lock->lock();
	_bolt_ipc_send(this->client_fd, &msg_type, sizeof(msg_type));
	_bolt_ipc_send(this->client_fd, &header, sizeof(header));
	this->send_lock->unlock();
}

void Browser::PluginWindow::SendCaptureDone() const {
	const BoltIPCMessageTypeToClient msg_type = IPC_MSG_EXTERNALCAPTUREDONE;
	const BoltIPCExternalCaptureDoneHeader header = { .window_id = this->window_id, .plugin_id = this->plugin_id };
	this->send_lock->lock();
	_bolt_ipc_send(this->client_fd, &msg_type, sizeof(msg_type));
	_bolt_ipc_send(this->client_fd, &header, sizeof(header));
	this->send_lock->unlock();
}

void Browser::PluginWindow::HandleShowDevtools() {
	this->ShowDevTools();
}

bool Browser::PluginWindow::OnBeforePopup(
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
	// block popup creation: plugin windows can't have child windows
	return true;
}

bool Browser::PluginWindow::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId process, CefRefPtr<CefProcessMessage> message) {
	if (!(this->browser && this->browser->IsSame(browser))) return false;
	const CefString name = message->GetName();
	if (name == "__bolt_plugin_capture_done") {
		this->SendCaptureDone();
		return true;
	}
	return false;
}

void Browser::PluginWindow::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
	Browser::Window::OnAfterCreated(browser);
	if (!this->pending_delete) {
		this->NotifyBrowserCreated(browser);
	}
}

CefRefPtr<CefRequestHandler> Browser::PluginWindow::GetRequestHandler() {
	return this;
}

void Browser::PluginWindow::Close() {
	this->closing = true;
	Browser::Window::Close();
}

bool Browser::PluginWindow::CanClose(CefRefPtr<CefWindow> win) {
	if (this->closing) {
		return Browser::Window::CanClose(win);
	}
	this->HandlePluginCloseRequest();
	return false;
}

void Browser::PluginWindow::NotifyClosed() {
	this->deleted = true;
	this->client->CleanupClientPlugins(this->client_fd);
}

#endif
