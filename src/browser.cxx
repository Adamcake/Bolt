#include "browser.hxx"
#include "include/views/cef_window.h"

#include <fmt/core.h>

Browser::Window::Window(CefRefPtr<CefClient> client, Browser::Details details, CefString url):
	closing(false), has_frame(details.controls_overlay), details(details), window(nullptr), browser_view(nullptr)
{
	CefBrowserSettings browser_settings;
	browser_settings.background_color = CefColorSetARGB(0, 0, 0, 0);
	if (details.controls_overlay) {
		CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
		dict->SetString("BoltAppUrl", url);
		// URL is unused, but still needs to be interpreted as valid, for some reason
		this->browser_view = CefBrowserView::CreateBrowserView(client, "0", browser_settings, std::move(dict), nullptr, this);
	} else {
		this->browser_view = CefBrowserView::CreateBrowserView(client, url, browser_settings, nullptr, nullptr, this);
	}
	CefWindow::CreateTopLevelWindow(this);
}

int Browser::Window::GetBrowserIdentifier() const {
	return this->browser_id;
}

void Browser::Window::ShowDevTools(CefRefPtr<CefClient> client) {
	CefWindowInfo window_info; // ignored, because this is a BrowserView
	CefBrowserSettings browser_settings;
	CefRect rect = this->window->GetClientAreaBoundsInScreen();
	CefPoint point(rect.width / 2, rect.height / 2);
	this->browser_view->GetBrowser()->GetHost()->ShowDevTools(window_info, client, browser_settings, point);
}

void Browser::Window::CloseRender() {
	this->browser_view->GetBrowser()->GetMainFrame()->SendProcessMessage(PID_RENDERER, CefProcessMessage::Create("__bolt_app_closing"));
}

void Browser::Window::CloseBrowser() {
	this->closing = true;
	this->window->Close();
}

bool Browser::Window::IsClosingWithHandle(int handle) const {
	return this->closing && this->browser_id == handle;
}

bool Browser::Window::HasFrame() const {
	return this->has_frame;
}

void Browser::Window::OnWindowCreated(CefRefPtr<CefWindow> window) {
	fmt::print("OnWindowCreated {}\n", window->GetID());
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

bool Browser::Window::CanClose(CefRefPtr<CefWindow>) {
	if (this->closing) {
		return true;
	} else {
		// CEF will call CefLifeSpanHandler::DoClose (implemented in Client), giving us a chance to
		// do cleanup, then set this->closing to true, then call Close() again.
		// This strategy is suggested by official examples eg cefsimple
		// https://github.com/chromiumembedded/cef/blob/5563/tests/cefsimple/simple_app.cc#L38-L45
		return this->browser_view->GetBrowser()->GetHost()->TryCloseBrowser();
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

void Browser::Window::OnBrowserCreated(CefRefPtr<CefBrowserView> browser_view, CefRefPtr<CefBrowser> browser) {
	fmt::print("OnBrowserCreated {}\n", browser->GetIdentifier());
	this->browser_id = browser->GetIdentifier();
}

cef_chrome_toolbar_type_t Browser::Window::GetChromeToolbarType() {
	return CEF_CTT_NONE;
}
