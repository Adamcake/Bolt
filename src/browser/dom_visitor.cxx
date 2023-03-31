#include "dom_visitor.hxx"

Browser::DOMVisitorAppFrame::DOMVisitorAppFrame(const CefString app_url): app_url(app_url) {

}

void Browser::DOMVisitorAppFrame::Visit(CefRefPtr<CefDOMDocument> document) {
	CefRefPtr<CefDOMNode> app_frame = document->GetElementById("app-frame");
	if(app_frame) {
		app_frame->SetElementAttribute("src", this->app_url);
	}
	CefRefPtr<CefDOMNode> button_close = document->GetElementById("button-close");
	if(button_close) {
		button_close->SetElementAttribute("onclick", "window.close()");
	}
	CefRefPtr<CefDOMNode> button_minify = document->GetElementById("button-minify");
	if(button_minify) {
		button_minify->SetElementAttribute("onclick", "window.__bolt_app_minify()");
	}
	CefRefPtr<CefDOMNode> button_settings = document->GetElementById("button-settings");
	if(button_settings) {
		button_settings->SetElementAttribute("onclick", "window.__bolt_app_settings()");
	}
	CefRefPtr<CefDOMNode> button_drag = document->GetElementById("button-drag");
	if(button_drag) {
		button_drag->SetElementAttribute("onmousedown", "window.__bolt_app_begin_drag()");
	}
}
