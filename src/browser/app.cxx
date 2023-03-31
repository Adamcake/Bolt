#include "app.hxx"
#include "dom_visitor.hxx"

#include <algorithm>

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
	fmt::print("[R] OnBrowserCreated for browser {}\n", browser->GetIdentifier());
	if (dict && dict->HasKey("BoltAppUrl")) {
		this->apps.push_back(new Browser::AppFrameData(browser->GetIdentifier(), dict->GetString("BoltAppUrl")));
	}
}

void Browser::App::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) {
	for (int i = 0; i < this->apps.size(); i += 1) {
		if (this->apps[i]->id == browser->GetIdentifier()) {
			if (this->apps[i]->url == frame->GetURL()) {
				fmt::print("[R] OnContextCreated for app view for browser {}\n", browser->GetIdentifier());
				CefRefPtr<CefV8Value> alt1 = CefV8Value::CreateObject(nullptr, nullptr);
				alt1->SetValue("identifyAppUrl", CefV8Value::CreateFunction("identifyAppUrl", this->apps[i]), V8_PROPERTY_ATTRIBUTE_READONLY);
				context->GetGlobal()->SetValue("alt1", alt1, V8_PROPERTY_ATTRIBUTE_READONLY);
			} else if (frame->IsMain()) {
				fmt::print("[R] OnContextCreated for app overlay for browser {}\n", browser->GetIdentifier());
				this->apps[i]->frame = frame;
				context->GetGlobal()->SetValue("__bolt_app_minify", CefV8Value::CreateFunction("__bolt_app_minify", this->apps[i]), V8_PROPERTY_ATTRIBUTE_READONLY);
				context->GetGlobal()->SetValue("__bolt_app_settings", CefV8Value::CreateFunction("__bolt_app_settings", this->apps[i]), V8_PROPERTY_ATTRIBUTE_READONLY);
				context->GetGlobal()->SetValue("__bolt_app_begin_drag", CefV8Value::CreateFunction("__bolt_app_begin_drag", this->apps[i]), V8_PROPERTY_ATTRIBUTE_READONLY);
			}
		}
	}
}

void Browser::App::OnUncaughtException(
	CefRefPtr<CefBrowser>,
	CefRefPtr<CefFrame>,
	CefRefPtr<CefV8Context>,
	CefRefPtr<CefV8Exception> exception,
	CefRefPtr<CefV8StackTrace> trace
) {
	fmt::print("[R] unhandled exception in {}: {}\n", exception->GetScriptResourceName().ToString(), exception->GetMessage().ToString());
	for (int i = 0; i < trace->GetFrameCount(); i += 1) {
		CefRefPtr<CefV8StackFrame> frame = trace->GetFrame(i);
		fmt::print("[R] in {}:{}:{}\n", frame->GetScriptNameOrSourceURL().ToString(), frame->GetLineNumber(), frame->GetColumn());
	}
	CefString source_line_ = exception->GetSourceLine();
	if (source_line_.size() <= 180) {
		std::string source_line = source_line_.ToString();
		fmt::print("[R] {}\n[R]", source_line);
		int i = 0;
		while (i < exception->GetStartColumn()) {
			fmt::print("-");
			i += 1;
		}
		while (i < exception->GetEndColumn()) {
			fmt::print("^");
			i += 1;
		}
		fmt::print("\n");
	} else {
		fmt::print("[R] <origin code not shown as it is too long ({} chars)>\n", source_line_.size());
	}
}

bool Browser::App::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame>, CefProcessId, CefRefPtr<CefProcessMessage> message) {
	if (message->GetName() == "__bolt_closing") {
		fmt::print("[R] bolt_closing received for browser {}\n", browser->GetIdentifier());
		this->apps.erase(
			std::remove_if(this->apps.begin(), this->apps.end(), [&browser](const CefRefPtr<Browser::AppFrameData>& data){ return browser->GetIdentifier() == data->id; })
		);
		return true;
	}

	return false;
}

void Browser::App::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int) {
	if (CefCurrentlyOn(TID_RENDERER)) {
		if (frame->IsMain()) {
			fmt::print("[R] OnLoadEnd for browser {}\n", browser->GetIdentifier());
			for(int i = 0; i < this->apps.size(); i += 1) {
				if (this->apps[i]->id == browser->GetIdentifier()) {
					frame->VisitDOM(new DOMVisitorAppFrame(this->apps[i]->url));
				}
			}
		}
	}
}

void Browser::App::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode, const CefString&, const CefString&) {
	if (CefCurrentlyOn(TID_RENDERER)) {
		this->apps.erase(
			std::remove_if(this->apps.begin(), this->apps.end(), [&browser](const CefRefPtr<Browser::AppFrameData>& data){ return browser->GetIdentifier() == data->id; })
		);
	}
}
