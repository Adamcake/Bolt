#include "browser_view_delegate.hxx"

Browser::BrowserViewDelegate::BrowserViewDelegate(Browser::Details details): details(details) {
	
}

CefSize Browser::BrowserViewDelegate::GetPreferredSize(CefRefPtr<CefView>) {
	return CefSize(this->details.preferred_width, this->details.preferred_height);
}

CefSize Browser::BrowserViewDelegate::GetMinimumSize(CefRefPtr<CefView>) {
	return CefSize(this->details.min_width, this->details.min_height);
}

CefSize Browser::BrowserViewDelegate::GetMaximumSize(CefRefPtr<CefView>) {
	return CefSize(this->details.max_width, this->details.max_height);
}

cef_chrome_toolbar_type_t Browser::BrowserViewDelegate::GetChromeToolbarType() {
	return CEF_CTT_NONE;
}
