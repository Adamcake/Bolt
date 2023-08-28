#include "client.hxx"
#include "include/cef_app.h"
#include "window_launcher.hxx"

#include "include/cef_life_span_handler.h"

#include <algorithm>
#include <fcntl.h>
#include <fmt/core.h>
#include <fstream>
#include <spawn.h>

constexpr Browser::Details LAUNCHER_DETAILS = {
	.preferred_width = 800,
	.preferred_height = 608,
	.center_on_open = true,
	.resizeable = true,
	.frame = true,
	.controls_overlay = false,
};

Browser::Client::Client(CefRefPtr<Browser::App> app,std::filesystem::path config_dir, std::filesystem::path data_dir):
	is_closing(false), show_devtools(true), config_dir(config_dir), data_dir(data_dir)
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

#if defined(CEF_X11)
	this->xcb = xcb_connect(nullptr, nullptr);
#endif
}

void Browser::Client::OpenLauncher() {
	std::lock_guard<std::mutex> _(this->windows_lock);
	auto it = std::find_if(this->windows.begin(), this->windows.end(), [](CefRefPtr<Window>& win) { return win->IsLauncher(); });
	if (it == this->windows.end()) {
		CefRefPtr<Window> w = new Launcher(this, LAUNCHER_DETAILS, this->show_devtools, &this->internal_pages, this->config_dir, this->data_dir);
		this->windows.push_back(w);
	} else {
		(*it)->Focus();
	}
}

void Browser::Client::Exit() {
	fmt::print("[B] Exit\n");
	if (this->windows.size() == 0) {
		CefQuitMessageLoop();
#if defined(CEF_X11)
		xcb_disconnect(this->xcb);
#endif
	} else {
		this->is_closing = true;
		this->closing_windows_remaining = 0;
		for (CefRefPtr<Browser::Window>& window: this->windows) {
			this->closing_windows_remaining += window->CountBrowsers();
			window->Close();
		}
		this->windows.clear();
	}
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
	std::lock_guard<std::mutex> _(this->windows_lock);
	CefRefPtr<Browser::Window> w = new Browser::Launcher(this, LAUNCHER_DETAILS, this->show_devtools, &this->internal_pages, this->config_dir, this->data_dir);
	this->windows.push_back(w);
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

	// if closing, Exit() is already holding the mutex lock, plus there's no need to clean up
	// individual windows when all are being closed at once
	if (!this->is_closing) {
		std::lock_guard<std::mutex> _(this->windows_lock);
		this->windows.erase(
			std::remove_if(
				this->windows.begin(),
				this->windows.end(),
				[&browser](const CefRefPtr<Browser::Window>& window){ return window->OnBrowserClosing(browser); }
			),
			this->windows.end()
		);
	}
	return false;
}

void Browser::Client::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] OnBeforeClose for browser {}, remaining windows {}\n", browser->GetIdentifier(), this->windows.size());
	if (this->is_closing) {
		this->closing_windows_remaining -= 1;
		if (this->closing_windows_remaining == 0) {
			// retry Exit(), hoping this time windows.size() will be 0
			this->Exit();
		}
	}
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

	if (name == "__bolt_close") {
		fmt::print("[B] bolt_close message received, exiting\n");
		this->Exit();
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

// oh no
// clangd: NOLINTNEXTLINE
constexpr unsigned char tray_icon[] = { 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 72, 255, 255, 255, 135, 255, 255, 255, 136, 255, 255, 255, 88, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 80, 255, 255, 255, 169, 255, 255, 255, 223, 255, 255, 255, 219, 255, 255, 255, 72, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 186, 255, 255, 255, 247, 255, 255, 255, 245, 255, 255, 255, 242, 255, 255, 255, 240, 255, 255, 255, 237, 255, 255, 255, 237, 255, 255, 255, 245, 255, 255, 255, 247, 255, 255, 255, 244, 255, 255, 255, 249, 255, 255, 255, 124, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 194, 255, 255, 255, 237, 255, 255, 255, 140, 255, 255, 255, 160, 255, 255, 255, 176, 255, 255, 255, 189, 255, 255, 255, 195, 255, 255, 255, 171, 255, 255, 255, 122, 255, 255, 255, 171, 255, 255, 255, 246, 255, 255, 255, 108, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 179, 255, 255, 255, 243, 255, 255, 255, 86, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 183, 255, 255, 255, 243, 255, 255, 255, 75, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 152, 255, 255, 255, 246, 255, 255, 255, 122, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 197, 255, 255, 255, 244, 255, 255, 255, 77, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 142, 255, 255, 255, 243, 255, 255, 255, 119, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 229, 255, 255, 255, 230, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 157, 255, 255, 255, 241, 255, 255, 255, 105, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 70, 255, 255, 255, 244, 255, 255, 255, 187, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 189, 255, 255, 255, 239, 255, 255, 255, 75, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 150, 255, 255, 255, 246, 255, 255, 255, 141, 255, 255, 255, 56, 255, 255, 255, 144, 255, 255, 255, 48, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 205, 255, 255, 255, 230, 255, 255, 255, 28, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 215, 255, 255, 255, 240, 255, 255, 255, 106, 255, 255, 255, 215, 255, 255, 255, 253, 255, 255, 255, 170, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 224, 255, 255, 255, 222, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 100, 255, 255, 255, 248, 255, 255, 255, 238, 255, 255, 255, 242, 255, 255, 255, 249, 255, 255, 255, 250, 255, 255, 255, 160, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 229, 255, 255, 255, 202, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 128, 255, 255, 255, 250, 255, 255, 255, 245, 255, 255, 255, 218, 255, 255, 255, 196, 255, 255, 255, 245, 255, 255, 255, 117, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 85, 255, 255, 255, 243, 255, 255, 255, 189, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 16, 255, 255, 255, 98, 255, 255, 255, 66, 255, 255, 255, 32, 255, 255, 255, 233, 255, 255, 255, 231, 255, 255, 255, 46, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 159, 255, 255, 255, 245, 255, 255, 255, 128, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 163, 255, 255, 255, 250, 255, 255, 255, 175, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 219, 255, 255, 255, 237, 255, 255, 255, 53, 255, 255, 255, 0, 255, 255, 255, 85, 255, 255, 255, 225, 255, 255, 255, 199, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 231, 255, 255, 255, 237, 255, 255, 255, 58, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 32, 255, 255, 255, 243, 255, 255, 255, 207, 255, 255, 255, 89, 255, 255, 255, 193, 255, 255, 255, 244, 255, 255, 255, 255, 255, 255, 255, 226, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 111, 255, 255, 255, 246, 255, 255, 255, 172, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 130, 255, 255, 255, 250, 255, 255, 255, 231, 255, 255, 255, 244, 255, 255, 255, 248, 255, 255, 255, 236, 255, 255, 255, 248, 255, 255, 255, 127, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 227, 255, 255, 255, 243, 255, 255, 255, 86, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 171, 255, 255, 255, 249, 255, 255, 255, 243, 255, 255, 255, 204, 255, 255, 255, 120, 255, 255, 255, 168, 255, 255, 255, 243, 255, 255, 255, 94, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 132, 255, 255, 255, 250, 255, 255, 255, 192, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 62, 255, 255, 255, 122, 255, 255, 255, 68, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 183, 255, 255, 255, 243, 255, 255, 255, 80, 255, 255, 255, 0, 255, 255, 255, 105, 255, 255, 255, 243, 255, 255, 255, 236, 255, 255, 255, 68, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 209, 255, 255, 255, 239, 255, 255, 255, 46, 255, 255, 255, 0, 255, 255, 255, 222, 255, 255, 255, 247, 255, 255, 255, 123, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 232, 255, 255, 255, 221, 255, 255, 255, 51, 255, 255, 255, 202, 255, 255, 255, 250, 255, 255, 255, 188, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 77, 255, 255, 255, 245, 255, 255, 255, 187, 255, 255, 255, 218, 255, 255, 255, 251, 255, 255, 255, 206, 255, 255, 255, 28, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 169, 255, 255, 255, 250, 255, 255, 255, 241, 255, 255, 255, 248, 255, 255, 255, 194, 255, 255, 255, 40, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 218, 255, 255, 255, 253, 255, 255, 255, 241, 255, 255, 255, 160, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 177, 255, 255, 255, 197, 255, 255, 255, 86, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0 };
const unsigned char* const Browser::Client::GetTrayIcon() const {
	return tray_icon;
}
