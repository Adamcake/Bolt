#include "render_process_handler.hxx"
#include "../dom_visitor.hxx"

Browser::RenderProcessHandler::RenderProcessHandler() {
	
}

void Browser::RenderProcessHandler::OnBrowserCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDictionaryValue> dict) {
	if (dict->HasKey("BoltAppUrl")) {
		this->pending_app_frames[browser->GetIdentifier()] = dict->GetString("BoltAppUrl");
	}
}

void Browser::RenderProcessHandler::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) {
	this->pending_app_frames.erase(browser->GetIdentifier());
}

void Browser::RenderProcessHandler::OnUncaughtException(
	CefRefPtr<CefBrowser>,
	CefRefPtr<CefFrame>,
	CefRefPtr<CefV8Context>,
	CefRefPtr<CefV8Exception>,
	CefRefPtr<CefV8StackTrace>
) {

}

CefRefPtr<CefLoadHandler> Browser::RenderProcessHandler::GetLoadHandler() {
	return this;
}

void Browser::RenderProcessHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int) {
	auto it = this->pending_app_frames.find(browser->GetIdentifier());
	if (it != this->pending_app_frames.end() && frame->IsMain()) {
		frame->VisitDOM(new DOMVisitorAppFrame(it->second));
		this->pending_app_frames.erase(it);
	}
}
