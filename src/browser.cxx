#include "browser.hxx"
#include "browser/browser_view_delegate.hxx"
#include "include/views/cef_window.h"

Browser::Window::Window(CefRefPtr<CefClient> client, Browser::Details details) {
	CefBrowserSettings browser_settings;
	browser_settings.background_color = CefColorSetARGB(0xFF, 0xFF, 0xFF, 0xFF);
	CefRefPtr<CefBrowserViewDelegate> bvd = new Browser::BrowserViewDelegate(details);
	CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(client, "https://adamcake.com", browser_settings, nullptr, nullptr, bvd);
	CefRefPtr<CefBrowserView> controls_overlay = nullptr;
	if (details.controls_overlay) {
		browser_settings.background_color = CefColorSetARGB(0, 0, 0, 0);
		controls_overlay = CefBrowserView::CreateBrowserView(client, "http://bolt/app-overlay", browser_settings, nullptr, nullptr, bvd);
	}
	this->window_delegate = new Browser::WindowDelegate(std::move(browser_view), std::move(bvd), std::move(controls_overlay), details);
	CefWindow::CreateTopLevelWindow(this->window_delegate);
}

Browser::Window::~Window() {
	this->window_delegate->Close();
}
