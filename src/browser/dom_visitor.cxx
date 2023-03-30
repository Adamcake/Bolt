#include "dom_visitor.hxx"

Browser::DOMVisitorAppFrame::DOMVisitorAppFrame(const CefString app_url): app_url(app_url) {

}

void Browser::DOMVisitorAppFrame::Visit(CefRefPtr<CefDOMDocument> document) {
	CefRefPtr<CefDOMNode> app_frame = document->GetElementById("app-frame");
	if(app_frame) {
		app_frame->SetElementAttribute("src", this->app_url);
	}
}
