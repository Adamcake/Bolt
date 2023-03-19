#include "client.hxx"

Browser::Client::Client(CefRefPtr<CefLifeSpanHandler> life_span_handler) {
	this->life_span_handler = life_span_handler;
}

CefRefPtr<CefLifeSpanHandler> Browser::Client::GetLifeSpanHandler() {
	return this->life_span_handler;
}
