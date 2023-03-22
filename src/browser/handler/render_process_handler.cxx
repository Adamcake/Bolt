#include "render_process_handler.hxx"

Browser::RenderProcessHandler::RenderProcessHandler() {
	
}

void Browser::RenderProcessHandler::OnBrowserCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDictionaryValue> dict) {
	
}

void Browser::RenderProcessHandler::OnBrowserDestroyed(CefRefPtr<CefBrowser>) {

}

void Browser::RenderProcessHandler::OnContextCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefV8Context>) {
	
}

void Browser::RenderProcessHandler::OnContextReleased(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefV8Context>) {

}

void Browser::RenderProcessHandler::OnUncaughtException(
	CefRefPtr<CefBrowser>,
	CefRefPtr<CefFrame>,
	CefRefPtr<CefV8Context>,
	CefRefPtr<CefV8Exception>,
	CefRefPtr<CefV8StackTrace>
) {

}
