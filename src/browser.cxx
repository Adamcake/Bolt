#include "browser.hxx"
#include "browser/browser_view_delegate.hxx"
#include "include/views/cef_window.h"

Browser::Window::Window(CefRefPtr<CefClient> client, Browser::Details details): closing(false) {
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

int Browser::Window::GetBrowserIdentifier() const {
	return this->window_delegate->GetBrowserIdentifier();
}

void Browser::Window::CloseRender() {
	this->window_delegate->SendProcessMessage(PID_RENDERER, CefProcessMessage::Create("__bolt_app_closing"));
}

void Browser::Window::CloseBrowser() {
	this->closing_handle = this->window_delegate->GetBrowserIdentifier();
	this->window_delegate->Close();
	this->closing = true;
}

bool Browser::Window::IsClosingWithHandle(int handle) const {
	return this->closing && this->closing_handle == handle;
}
