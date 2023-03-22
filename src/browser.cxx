#include "browser.hxx"
#include "browser/browser_view_delegate.hxx"
#include "include/views/cef_window.h"

Browser::Window::Window(CefRefPtr<CefClient> client, Browser::Details details) {
	CefBrowserSettings browser_settings;
	CefRefPtr<CefBrowserViewDelegate> bvd = new Browser::BrowserViewDelegate(details);
	CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(client, "https://adamcake.com", browser_settings, nullptr, nullptr, bvd);
	CefRefPtr<CefBrowserView> controls_overlay = nullptr;
	if (details.controls_overlay) {
		CefRefPtr<CefBrowserViewDelegate> bvd = new Browser::BrowserViewDelegate(details); // TODO: is this line necessary?
		CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
		dict->SetNull("BoltMakeControlsOverlay");
		controls_overlay = CefBrowserView::CreateBrowserView(client, "", browser_settings, std::move(dict), nullptr, bvd);
	}
	this->window_delegate = new Browser::WindowDelegate(std::move(browser_view), std::move(bvd), std::move(controls_overlay), details);
	CefWindow::CreateTopLevelWindow(this->window_delegate);
}

Browser::Window::~Window() {
	this->window_delegate->Close();
}
