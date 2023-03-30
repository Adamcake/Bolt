#include "app.hxx"
#include "dom_visitor.hxx"

Browser::App::App() {
	
}

CefRefPtr<CefRenderProcessHandler> Browser::App::GetRenderProcessHandler() {
	return this;
}

CefRefPtr<CefLoadHandler> Browser::App::GetLoadHandler() {
	return this;
}

void Browser::App::OnBrowserCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDictionaryValue> dict) {
	if (dict && dict->HasKey("BoltAppUrl")) {
		this->pending_app_frames[browser->GetIdentifier()] = dict->GetString("BoltAppUrl");
	}
}

void Browser::App::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) {
	this->pending_app_frames.erase(browser->GetIdentifier());
}

void Browser::App::OnUncaughtException(
	CefRefPtr<CefBrowser>,
	CefRefPtr<CefFrame>,
	CefRefPtr<CefV8Context>,
	CefRefPtr<CefV8Exception>,
	CefRefPtr<CefV8StackTrace>
) {

}

void Browser::App::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int) {
	if (frame->IsMain()) {
		auto it = this->pending_app_frames.find(browser->GetIdentifier());
		if (it != this->pending_app_frames.end()) {
			frame->VisitDOM(new DOMVisitorAppFrame(it->second));
			this->pending_app_frames.erase(it);
		}
	}
}

void Browser::App::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode, const CefString&, const CefString&) {
	if (frame->IsMain()) {
		auto it = this->pending_app_frames.find(browser->GetIdentifier());
		if (it != this->pending_app_frames.end()) {
			this->pending_app_frames.erase(it);
		}
	}
}
