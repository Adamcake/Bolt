#include "browser.hxx"
#include "include/views/cef_window.h"

#include <fmt/core.h>

Browser::Window::Window(CefRefPtr<CefClient> client, Browser::Details details, CefString url):
	details(details), window(nullptr), browser_view(nullptr)
{
	CefBrowserSettings browser_settings;
	browser_settings.background_color = CefColorSetARGB(0, 0, 0, 0);
	if (details.controls_overlay) {
		CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
		dict->SetString("BoltAppUrl", url);
		this->browser_view = CefBrowserView::CreateBrowserView(client, "https://bolt-internal/frame.html", browser_settings, std::move(dict), nullptr, this);
	} else {
		this->browser_view = CefBrowserView::CreateBrowserView(client, url, browser_settings, nullptr, nullptr, this);
	}
	CefWindow::CreateTopLevelWindow(this);
}

void Browser::Window::OnWindowCreated(CefRefPtr<CefWindow> window) {
	fmt::print("[B] OnWindowCreated {}\n", window->GetID());
	this->window = std::move(window);
	this->window->AddChildView(this->browser_view);
	this->window->Show();
}

void Browser::Window::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
	this->window = nullptr;
	this->browser_view = nullptr;
}

CefRect Browser::Window::GetInitialBounds(CefRefPtr<CefWindow> window) {
	return cef_rect_t {
		.x = this->details.startx,
		.y = this->details.starty,
		.width = this->details.preferred_width,
		.height = this->details.preferred_height,
	};
}

cef_show_state_t Browser::Window::GetInitialShowState(CefRefPtr<CefWindow>) {
	return CEF_SHOW_STATE_NORMAL;
}

bool Browser::Window::IsFrameless(CefRefPtr<CefWindow>) {
	return !this->details.frame;
}

bool Browser::Window::CanResize(CefRefPtr<CefWindow>) {
	return this->details.resizeable;
}

bool Browser::Window::CanMaximize(CefRefPtr<CefWindow>) {
	return false;
}

bool Browser::Window::CanMinimize(CefRefPtr<CefWindow>) {
	return false;
}

bool Browser::Window::CanClose(CefRefPtr<CefWindow> win) {
	// CEF will call CefLifeSpanHandler::DoClose (implemented in Client), giving us a chance to
	// do cleanup and then call TryCloseBrowser() a second time.
	// This strategy is suggested by official examples, e.g. cefsimple:
	// https://github.com/chromiumembedded/cef/blob/5563/tests/cefsimple/simple_app.cc#L38-L45
	CefRefPtr<CefBrowser> browser = this->browser_view->GetBrowser();
	if (browser) {
		return browser->GetHost()->TryCloseBrowser();
	} else {
		return true;
	}
}

CefSize Browser::Window::GetPreferredSize(CefRefPtr<CefView>) {
	return CefSize(this->details.preferred_width, this->details.preferred_height);
}

CefSize Browser::Window::GetMinimumSize(CefRefPtr<CefView>) {
	return CefSize(this->details.min_width, this->details.min_height);
}

CefSize Browser::Window::GetMaximumSize(CefRefPtr<CefView>) {
	return CefSize(this->details.max_width, this->details.max_height);
}

void Browser::Window::OnBrowserCreated(CefRefPtr<CefBrowserView>, CefRefPtr<CefBrowser>) {

}

cef_chrome_toolbar_type_t Browser::Window::GetChromeToolbarType() {
	return CEF_CTT_NONE;
}
