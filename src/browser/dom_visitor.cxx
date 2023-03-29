#include "dom_visitor.hxx"

Browser::DOMVisitorAppFrame::DOMVisitorAppFrame(const CefString app_url): app_url(app_url) {

}

void Browser::DOMVisitorAppFrame::Visit(CefRefPtr<CefDOMDocument> document) {
	document->GetElementById("app-frame")->SetElementAttribute("src", this->app_url);
}
