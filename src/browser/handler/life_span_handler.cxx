#include "life_span_handler.hxx"

#include "include/cef_app.h"

Browser::LifeSpanHandler::LifeSpanHandler() {
	
}

void Browser::LifeSpanHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	CefQuitMessageLoop();
}
