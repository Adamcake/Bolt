#include "browser.hxx"
#include "src/browser/browser_view_delegate.hxx"

#include "include/views/cef_window.h"

Browser::Window::Window(CefRefPtr<CefClient> client, Browser::Details details) {
	CefBrowserSettings browser_settings;
	CefRefPtr<CefBrowserViewDelegate> bvd = new Browser::BrowserViewDelegate(details);
	CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(client, "https://adamcake.com", browser_settings, nullptr, nullptr, bvd);
	this->window_delegate = new Browser::WindowDelegate(std::move(browser_view), std::move(bvd), details);
	CefWindow::CreateTopLevelWindow(this->window_delegate);
}

Browser::Window::~Window() {
	this->window_delegate->Close();
}
