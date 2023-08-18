#include "client.hxx"
#include "window_launcher.hxx"

#include "include/cef_life_span_handler.h"

#include <algorithm>
#include <fcntl.h>
#include <fmt/core.h>
#include <fstream>
#include <spawn.h>

Browser::Client::Client(CefRefPtr<Browser::App> app,std::filesystem::path config_dir, std::filesystem::path data_dir):
	show_devtools(true), config_dir(config_dir), data_dir(data_dir)
{
	CefString mime_type_html = "text/html";
	CefString mime_type_js = "application/javascript";
	CefString mime_type_css = "text/css";
	app->SetBrowserProcessHandler(this);
	this->internal_pages["/launcher.html"] = InternalFile("html/launcher.html", mime_type_html);
	this->internal_pages["/launcher.js"] = InternalFile("html/launcher.js", mime_type_js);
	this->internal_pages["/launcher.css"] = InternalFile("html/launcher.css", mime_type_css);
	this->internal_pages["/oauth.html"] = InternalFile("html/oauth.html", mime_type_html);
	this->internal_pages["/game_auth.html"] = InternalFile("html/game_auth.html", mime_type_html);
	this->internal_pages["/frame.html"] = InternalFile("html/frame.html", mime_type_html);
}

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
	Browser::Details details = {
		.preferred_width = 800,
		.preferred_height = 608,
		.center_on_open = true,
		.resizeable = true,
		.frame = true,
		.controls_overlay = false,
	};

	this->windows_lock.lock();
	CefRefPtr<Browser::Window> w = new Browser::Launcher(this, details, this->show_devtools, &this->internal_pages, this->data_dir);
	this->windows.push_back(w);
	this->windows_lock.unlock();
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
}

bool Browser::Client::DoClose(CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] DoClose for browser {}\n", browser->GetIdentifier());
	std::lock_guard<std::mutex> _(this->windows_lock);
	this->windows.erase(
		std::remove_if(
			this->windows.begin(),
			this->windows.end(),
			[&browser](const CefRefPtr<Browser::Window>& window){ return window->CloseBrowser(browser); }
		),
		this->windows.end()
	);
	return false;
}

void Browser::Client::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] OnBeforeClose for browser {}\n", browser->GetIdentifier());
}

bool Browser::Client::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId, CefRefPtr<CefProcessMessage> message) {
	std::lock_guard<std::mutex> _(this->windows_lock);
	CefString name = message->GetName();

	if (name == "__bolt_app_settings") {
		fmt::print("[B] bolt_app_settings received for browser {}\n", browser->GetIdentifier());
		CefWindowInfo window_info; // ignored, because this is a BrowserView
		CefBrowserSettings browser_settings;
		browser->GetHost()->ShowDevTools(window_info, this, browser_settings, CefPoint());
		return true;
	}

	if (name == "__bolt_app_minify") {
		fmt::print("[B] bolt_app_minify received for browser {}\n", browser->GetIdentifier());
		return true;
	}

	if (name == "__bolt_app_begin_drag") {
		fmt::print("[B] bolt_app_begin_drag received for browser {}\n", browser->GetIdentifier());
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
	// Find the Window responsible for this request, if any
	std::lock_guard<std::mutex> _(this->windows_lock);
	auto it = std::find_if(
		this->windows.begin(),
		this->windows.end(),
		[&browser](const CefRefPtr<Browser::Window>& window){ return window->HasBrowser(browser); }
	);
	if (it == this->windows.end()) {
		// Request came from no window?
		return nullptr;
	} else {
		return (*it)->GetResourceRequestHandler(browser, frame, request, is_navigation, is_download, request_initiator, disable_default_handling);
	}
}
