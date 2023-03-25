#include "request_handler.hxx"

Browser::RequestHandler::RequestHandler() {
	
}

CefRefPtr<CefResourceRequestHandler> Browser::RequestHandler::GetResourceRequestHandler(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	bool is_navigation,
	bool is_download,
	const CefString& request_initiator,
	bool& disable_default_handling
) {
	// TODO: handle custom URLs here such as http://bolt/overlay
	return nullptr;
}
