#include "app.hxx"
#include "dom_visitor.hxx"

#include <fmt/core.h>

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

void Browser::App::OnContextCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context>) {
	frame->GetV8Context()->GetGlobal()->SetValue("alt1", CefV8Value::CreateBool(false), V8_PROPERTY_ATTRIBUTE_READONLY);
}

void Browser::App::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) {
	this->pending_app_frames.erase(browser->GetIdentifier());
}

void Browser::App::OnUncaughtException(
	CefRefPtr<CefBrowser>,
	CefRefPtr<CefFrame>,
	CefRefPtr<CefV8Context>,
	CefRefPtr<CefV8Exception> exception,
	CefRefPtr<CefV8StackTrace> trace
) {
	fmt::print("Unhandled exception in {}: {}\n", exception->GetScriptResourceName().ToString(), exception->GetMessage().ToString());
	for (int i = 0; i < trace->GetFrameCount(); i += 1) {
		CefRefPtr<CefV8StackFrame> frame = trace->GetFrame(i);
		fmt::print("In {}:{}:{}\n", frame->GetScriptNameOrSourceURL().ToString(), frame->GetLineNumber(), frame->GetColumn());
	}
	CefString source_line_ = exception->GetSourceLine();
	if (source_line_.size() <= 180) {
		std::string source_line = source_line_.ToString();
		fmt::print("{}\n", source_line);
		int i = 0;
		while (i < exception->GetStartColumn()) {
			fmt::print("-");
			i += 1;
		}
		while (i < exception->GetEndColumn()) {
			fmt::print("^");
			i += 1;
		}
	} else {
		fmt::print("<origin code not shown as it is too long ({} chars)>\n", source_line_.size());
	}
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
