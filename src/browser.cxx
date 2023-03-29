#include "browser.hxx"
#include "browser/browser_view_delegate.hxx"
#include "include/views/cef_window.h"

Browser::Window::Window(CefRefPtr<CefClient> client, Browser::Details details) {
	CefString url = "https://adamcake.com/";
	CefBrowserSettings browser_settings;
	browser_settings.background_color = CefColorSetARGB(0, 0, 0, 0);
	CefRefPtr<CefBrowserViewDelegate> bvd = new Browser::BrowserViewDelegate(details);
	CefRefPtr<CefBrowserView> browser_view;
	if (details.controls_overlay) {
		CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
		dict->SetString("BoltAppUrl", url);
		browser_view = CefBrowserView::CreateBrowserView(client, "http://bolt/app", browser_settings, std::move(dict), nullptr, bvd);
	} else {
		browser_view = CefBrowserView::CreateBrowserView(client, url, browser_settings, nullptr, nullptr, bvd);
	}
	this->window_delegate = new Browser::WindowDelegate(std::move(browser_view), std::move(bvd), details);
	CefWindow::CreateTopLevelWindow(this->window_delegate);
}

Browser::Window::~Window() {
	this->window_delegate->Close();
}
