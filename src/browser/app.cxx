#include "app.hxx"

#include <algorithm>

#include <fmt/core.h>

/*
Any attempt to leverage the render process has resulted in a display of the truly staggering incompetence with
which Chromium was developed. It is just about unusable. Despite CefRenderProcessHandler being the official way
to do interop, Most CEF-based applications do interop by web requests that they can intercept in the browser
process, and having tried to do it this way, I now understand why that is. Consider this your only warning:
DO NOT try to use CefRenderProcessHandler for anything; every single one of its methods is BROKEN BEYOND BELIEF.
Your time is valuable, don't waste it here.
*/

Browser::App::App(): browser_process_handler(nullptr) {
	
}

void Browser::App::SetBrowserProcessHandler(CefRefPtr<CefBrowserProcessHandler> handler) {
	this->browser_process_handler = handler;
}

CefRefPtr<CefRenderProcessHandler> Browser::App::GetRenderProcessHandler() {
	return this;
}

CefRefPtr<CefBrowserProcessHandler> Browser::App::GetBrowserProcessHandler() {
	// May be null, but in theory it should never be null on the main thread because it gets set via
	// SetBrowserProcessHandler() before CefInitialize() is called. In other processes it will be null.
	return this->browser_process_handler;
}

CefRefPtr<CefLoadHandler> Browser::App::GetLoadHandler() {
	return this;
}

void Browser::App::OnBrowserCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDictionaryValue> dict) {
	fmt::print("[R] OnBrowserCreated for browser {}\n", browser->GetIdentifier());
}

void Browser::App::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) {
	context->GetGlobal()->SetValue("s", CefV8Value::CreateFunction("s", this), V8_PROPERTY_ATTRIBUTE_READONLY);
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

bool Browser::App::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId, CefRefPtr<CefProcessMessage> message) {
	return false;
}

void Browser::App::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int) {
	if (CefCurrentlyOn(TID_RENDERER)) {
		if (frame->IsMain()) {
			fmt::print("[R] OnLoadEnd for browser {}\n", browser->GetIdentifier());
		}
	}
}

void Browser::App::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode, const CefString&, const CefString&) {
	fmt::print("[R] OnLoadError\n");
}

bool Browser::App::Execute(const CefString&, CefRefPtr<CefV8Value>, const CefV8ValueList&, CefRefPtr<CefV8Value>& retval, CefString&) {
	retval = CefV8Value::CreateObject(nullptr, nullptr);
	retval->SetValue("origin", CefV8Value::CreateString("aHR0cHM6Ly9hY2NvdW50LmphZ2V4LmNvbQ"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("redirect", CefV8Value::CreateString("aHR0cHM6Ly9zZWN1cmUucnVuZXNjYXBlLmNvbS9tPXdlYmxvZ2luL2xhdW5jaGVyLXJlZGlyZWN0"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("clientid", CefV8Value::CreateString("Y29tX2phZ2V4X2F1dGhfZGVza3RvcF9sYXVuY2hlcg"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("api", CefV8Value::CreateString("aHR0cHM6Ly9hcGkuamFnZXguY29tL3Yx"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("auth_api", CefV8Value::CreateString("aHR0cHM6Ly9hdXRoLmphZ2V4LmNvbS9nYW1lLXNlc3Npb24vdjE"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("profile_api", CefV8Value::CreateString("aHR0cHM6Ly9zZWN1cmUuamFnZXguY29tL3JzLXByb2ZpbGUvdjE"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("shield_url", CefV8Value::CreateString("aHR0cHM6Ly9hdXRoLmphZ2V4LmNvbS9zaGllbGQvb2F1dGgvdG9rZW4"), V8_PROPERTY_ATTRIBUTE_READONLY);
	return true;
}
