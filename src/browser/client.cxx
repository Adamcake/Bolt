#include "client.hxx"
#include "include/cef_app.h"
#include "window_launcher.hxx"

#include "include/cef_life_span_handler.h"

#include <algorithm>
#include <fcntl.h>
#include <fmt/core.h>
#include <fstream>

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
	.controls_overlay = false,
};

Browser::Client::Client(CefRefPtr<Browser::App> app,std::filesystem::path config_dir, std::filesystem::path data_dir):
#if defined(BOLT_DEV_LAUNCHER_DIRECTORY)
	CLIENT_FILEHANDLER(BOLT_DEV_LAUNCHER_DIRECTORY),
#endif
	is_closing(false), show_devtools(SHOW_DEVTOOLS), config_dir(config_dir), data_dir(data_dir)
{
	app->SetBrowserProcessHandler(this);

#if defined(CEF_X11)
	this->xcb = xcb_connect(nullptr, nullptr);
#endif
}

void Browser::Client::OpenLauncher() {
	std::lock_guard<std::mutex> _(this->windows_lock);
	auto it = std::find_if(this->windows.begin(), this->windows.end(), [](CefRefPtr<Window>& win) { return win->IsLauncher(); });
	if (it == this->windows.end()) {
		CefRefPtr<Window> w = new Launcher(this, LAUNCHER_DETAILS, this->show_devtools, this, this->config_dir, this->data_dir);
		this->windows.push_back(w);
	} else {
		(*it)->Focus();
	}
}

void Browser::Client::Exit() {
	fmt::print("[B] Exit\n");
	if (this->windows.size() == 0) {
		this->StopFileManager();
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

void Browser::Client::OnWindowCreated(CefRefPtr<CefWindow> window) {
	CefRefPtr<CefImage> image = CefImage::CreateImage();
	image->AddBitmap(1.0, 16, 16, CEF_COLOR_TYPE_RGBA_8888, CEF_ALPHA_TYPE_POSTMULTIPLIED, this->GetIcon16(), 16 * 16 * 4);
	window->SetWindowIcon(image);

	CefRefPtr<CefImage> image_big = CefImage::CreateImage();
	image_big->AddBitmap(1.0, 64, 64, CEF_COLOR_TYPE_RGBA_8888, CEF_ALPHA_TYPE_POSTMULTIPLIED, this->GetIcon64(), 64 * 64 * 4);
	window->SetWindowAppIcon(image_big);

#if defined(CEF_X11)
	constexpr char wm_class[] = "BoltLauncher\0BoltLauncher";
	const unsigned long handle = window->GetWindowHandle();
	xcb_change_property(this->xcb, XCB_PROP_MODE_REPLACE, handle, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, sizeof(wm_class), wm_class);
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
	std::lock_guard<std::mutex> _(this->windows_lock);
	CefRefPtr<Browser::Window> w = new Launcher(this, LAUNCHER_DETAILS, this->show_devtools, this, this->config_dir, this->data_dir);
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

	if (name == "__bolt_refresh") {
		fmt::print("[B] bolt_refresh message received, refreshing browser {}\n", browser->GetIdentifier());
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
