#include "app.hxx"
#include "include/internal/cef_types.h"

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
		fmt::print("[R] in {}:{}:{}\n", frame->GetScriptName().ToString(), frame->GetLineNumber(), frame->GetColumn());
	}

	constexpr int max_dist = 80;
	std::string source_line = exception->GetSourceLine().ToString();
	const size_t first_non_whitespace = source_line.find_first_not_of(" \t");
	const size_t last_non_whitespace = source_line.find_last_not_of(" \t");
	const std::string_view source_view_trimmed(source_line.begin() + first_non_whitespace, source_line.begin() + last_non_whitespace + 1);
	const int exc_start_column = exception->GetStartColumn() - first_non_whitespace;
	const int exc_end_column = exception->GetEndColumn() - first_non_whitespace;
	const bool do_trim_start = exc_start_column > max_dist;
	const bool do_trim_end = source_view_trimmed.size() - exc_end_column > max_dist;
	const std::string_view code_trimmed(
		source_view_trimmed.data() + (do_trim_start ? exc_start_column - max_dist : 0),
		source_view_trimmed.data() + (do_trim_end ? exc_end_column + max_dist : source_view_trimmed.size())
	);

	fmt::print("[R] {}{}{}\n[R] {}", do_trim_start ? "..." : "", code_trimmed, do_trim_end ? "..." : "", do_trim_start ? "---" : "");
	int i = 0;
	while (i < exc_start_column && i < max_dist) {
		fmt::print("-");
		i += 1;
	}
	i = 0;
	while (i < exc_end_column - exc_start_column) {
		fmt::print("^");
		i += 1;
	}
	fmt::print("\n");
}

bool Browser::App::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId, CefRefPtr<CefProcessMessage> message) {
	CefString name = message->GetName();

	if (name == "__bolt_close" || name == "__bolt_refresh") {
		frame->SendProcessMessage(PID_BROWSER, message);
		return true;
	}

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
	retval->SetValue("provider", CefV8Value::CreateString("cnVuZXNjYXBl"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("origin", CefV8Value::CreateString("aHR0cHM6Ly9hY2NvdW50LmphZ2V4LmNvbQ"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("origin_2fa", CefV8Value::CreateString("aHR0cHM6Ly9zZWN1cmUucnVuZXNjYXBlLmNvbQ"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("redirect", CefV8Value::CreateString("aHR0cHM6Ly9zZWN1cmUucnVuZXNjYXBlLmNvbS9tPXdlYmxvZ2luL2xhdW5jaGVyLXJlZGlyZWN0"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("clientid", CefV8Value::CreateString("Y29tX2phZ2V4X2F1dGhfZGVza3RvcF9sYXVuY2hlcg"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("api", CefV8Value::CreateString("aHR0cHM6Ly9hcGkuamFnZXguY29tL3Yx"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("auth_api", CefV8Value::CreateString("aHR0cHM6Ly9hdXRoLmphZ2V4LmNvbS9nYW1lLXNlc3Npb24vdjE"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("profile_api", CefV8Value::CreateString("aHR0cHM6Ly9zZWN1cmUuamFnZXguY29tL3JzLXByb2ZpbGUvdjE"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("shield_url", CefV8Value::CreateString("aHR0cHM6Ly9hdXRoLmphZ2V4LmNvbS9zaGllbGQvb2F1dGgvdG9rZW4"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("content_url", CefV8Value::CreateString("aHR0cHM6Ly9jb250ZW50LnJ1bmVzY2FwZS5jb20vZG93bmxvYWRzL3VidW50dS8"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("default_config_uri", CefV8Value::CreateString("aHR0cHM6Ly93d3cucnVuZXNjYXBlLmNvbS9rPTUvbD0wL2phdl9jb25maWcud3M"), V8_PROPERTY_ATTRIBUTE_READONLY);

	CefRefPtr<CefV8Value> games = CefV8Value::CreateArray(2);
	games->SetValue(0, CefV8Value::CreateString("UnVuZVNjYXBl"));
	games->SetValue(1, CefV8Value::CreateString("T2xkIFNjaG9vbA"));
	retval->SetValue("games", games, V8_PROPERTY_ATTRIBUTE_READONLY);
	return true;
}
