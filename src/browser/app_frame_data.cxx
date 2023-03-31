#include "app_frame_data.hxx"

#include <fmt/core.h>

bool Browser::AppFrameData::Execute(
	const CefString& name,
	CefRefPtr<CefV8Value> object,
	const CefV8ValueList& arguments,
	CefRefPtr<CefV8Value>& retval,
	CefString& exception
) {
	fmt::print("Callback {}\n", name.ToString());
	return true;
}
