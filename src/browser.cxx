#include "browser.hxx"
#include "src/browser/browser_view_delegate.hxx"

#include "include/views/cef_window.h"

Browser::Window Browser::Create(CefRefPtr<CefClient> client, Browser::Details details) {
	CefBrowserSettings browser_settings;
	CefRefPtr<CefBrowserViewDelegate> bvd = new Browser::BrowserViewDelegate(details);
	CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(client, "https://adamcake.com", browser_settings, nullptr, nullptr, bvd);
	CefRefPtr<WindowDelegate> window_delegate = new Browser::WindowDelegate(browser_view, bvd, details);
	CefWindow::CreateTopLevelWindow(window_delegate);
	return window_delegate;
}
