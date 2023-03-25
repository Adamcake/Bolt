#include "client.hxx"

#include <fmt/core.h>

Browser::Client::Client(CefRefPtr<CefLifeSpanHandler> life_span_handler, CefRefPtr<CefRequestHandler> request_handler):
	life_span_handler(life_span_handler), request_handler(request_handler) { }

CefRefPtr<CefLifeSpanHandler> Browser::Client::GetLifeSpanHandler() {
	return this->life_span_handler;
}

CefRefPtr<CefRequestHandler> Browser::Client::GetRequestHandler() {
	return this->request_handler;
}
