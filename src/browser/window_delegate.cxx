#include "window_delegate.hxx"
#include "include/views/cef_window.h"
#include "src/browser/browser_view_delegate.hxx"

Browser::WindowDelegate::WindowDelegate(
	CefRefPtr<CefBrowserView> browser_view,
	CefRefPtr<CefBrowserViewDelegate> browser_view_delegate,
	Details details
): details(details), window(nullptr), browser_view(browser_view), browser_view_delegate(browser_view_delegate) { }

void Browser::WindowDelegate::Close() {
	if (this->window) {
		this->window->Close();
		this->window = nullptr;
	}
}

void Browser::WindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) {
	this->window = std::move(window);
	this->window->AddChildView(this->browser_view);
	this->window->Show();
}

void Browser::WindowDelegate::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
	this->window = nullptr;
	this->browser_view = nullptr;
	this->browser_view_delegate = nullptr;
}

CefRect Browser::WindowDelegate::GetInitialBounds(CefRefPtr<CefWindow> window) {
	return cef_rect_t {
		.x = this->details.startx,
		.y = this->details.starty,
		.width = this->details.preferred_width,
		.height = this->details.preferred_height,
	};
}

cef_show_state_t Browser::WindowDelegate::GetInitialShowState(CefRefPtr<CefWindow>) {
	return CEF_SHOW_STATE_NORMAL;
}

bool Browser::WindowDelegate::IsFrameless(CefRefPtr<CefWindow>) {
	return !this->details.frame;
}

bool Browser::WindowDelegate::CanResize(CefRefPtr<CefWindow>) {
	return this->details.resizeable;
}

bool Browser::WindowDelegate::CanMaximize(CefRefPtr<CefWindow>) {
	return false;
}

bool Browser::WindowDelegate::CanMinimize(CefRefPtr<CefWindow>) {
	return false;
}

bool Browser::WindowDelegate::CanClose(CefRefPtr<CefWindow>) {
	return true;
}

CefSize Browser::WindowDelegate::GetPreferredSize(CefRefPtr<CefView>) {
	return CefSize(this->details.preferred_width, this->details.preferred_height);
}

CefSize Browser::WindowDelegate::GetMinimumSize(CefRefPtr<CefView>) {
	return CefSize(this->details.min_width, this->details.min_height);
}

CefSize Browser::WindowDelegate::GetMaximumSize(CefRefPtr<CefView>) {
	return CefSize(this->details.max_width, this->details.max_height);
}
