#include "app_frame_data.hxx"
#include "include/internal/cef_types.h"

#include <fmt/core.h>

bool Browser::AppFrameData::Execute(
	const CefString& name,
	CefRefPtr<CefV8Value> object,
	const CefV8ValueList& arguments,
	CefRefPtr<CefV8Value>& retval,
	CefString& exception
) {
	if (this->frame) {
		this->frame->SendProcessMessage(PID_BROWSER, CefProcessMessage::Create(name));
	}
	return true;
}
