#include "window_plugin.hxx"

#if defined(BOLT_PLUGINS)
#include "client.hxx"
#include "include/cef_base.h"

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

Browser::PluginWindow::PluginWindow(CefRefPtr<Client> main_client, Details details, const char* url, CefRefPtr<FileManager::Directory> file_manager, BoltSocketType fd, uint64_t id, uint64_t plugin_id, bool show_devtools):
	PluginRequestHandler(IPC_MSG_EXTERNALBROWSERMESSAGE), Window(main_client, details, show_devtools),
	file_manager(file_manager), client_fd(fd), window_id(id), plugin_id(plugin_id), deleted(false)
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

CefRefPtr<CefRequestHandler> Browser::PluginWindow::GetRequestHandler() {
	return this;
}

void Browser::PluginWindow::NotifyClosed() {
	this->deleted = true;
	this->client->CleanupClientPlugins(this->client_fd);
}

#endif
