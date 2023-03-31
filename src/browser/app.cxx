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
	if (dict && dict->HasKey("BoltAppUrl")) {
		this->apps.push_back(new Browser::AppFrameData(browser->GetIdentifier(), dict->GetString("BoltAppUrl")));
	}
}

void Browser::App::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) {
	for (int i = 0; i < this->apps.size(); i += 1) {
		if (this->apps[i]->id == browser->GetIdentifier()) {
			if (this->apps[i]->url == frame->GetURL()) {
				CefRefPtr<CefV8Value> alt1 = CefV8Value::CreateObject(nullptr, nullptr);
				alt1->SetValue("identifyAppUrl", CefV8Value::CreateFunction("identifyAppUrl", this->apps[i]), V8_PROPERTY_ATTRIBUTE_READONLY);
				context->GetGlobal()->SetValue("alt1", alt1, V8_PROPERTY_ATTRIBUTE_READONLY);
			} else if (frame->IsMain()) {
				context->GetGlobal()->SetValue("__bolt_close", CefV8Value::CreateFunction("__bolt_close", this->apps[i]), V8_PROPERTY_ATTRIBUTE_READONLY);
				context->GetGlobal()->SetValue("__bolt_minify", CefV8Value::CreateFunction("__bolt_minify", this->apps[i]), V8_PROPERTY_ATTRIBUTE_READONLY);
				context->GetGlobal()->SetValue("__bolt_settings", CefV8Value::CreateFunction("__bolt_settings", this->apps[i]), V8_PROPERTY_ATTRIBUTE_READONLY);
				context->GetGlobal()->SetValue("__bolt_begin_drag", CefV8Value::CreateFunction("__bolt_begin_drag", this->apps[i]), V8_PROPERTY_ATTRIBUTE_READONLY);
			}
		}

	}
	auto it = std::find_if(
		this->apps.begin(),
		this->apps.end(),
		[&browser, &frame](const CefRefPtr<Browser::AppFrameData>& data){ return data->id == browser->GetIdentifier() && frame->GetURL() == data->url; }
	);
	if (it != this->apps.end()) {
		CefRefPtr<CefV8Value> alt1 = CefV8Value::CreateObject(nullptr, nullptr);
		alt1->SetValue("identifyAppUrl", CefV8Value::CreateFunction("identifyAppUrl", *it), V8_PROPERTY_ATTRIBUTE_READONLY);
		// various others here...
		context->GetGlobal()->SetValue("alt1", alt1, V8_PROPERTY_ATTRIBUTE_READONLY);
	}
}

void Browser::App::OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context>) {
	if (frame->IsMain()) {
		this->OnBrowserDestroyed(browser);
	}
}

void Browser::App::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) {
	this->apps.erase(
		std::remove_if(this->apps.begin(), this->apps.end(), [&browser](const CefRefPtr<Browser::AppFrameData>& data){ return browser->GetIdentifier() == data->id; })
	);
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

bool Browser::App::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame>, CefProcessId, CefRefPtr<CefProcessMessage> message) {
	if (message->GetName() == "__bolt_closing") {
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
