#include "life_span_handler.hxx"

#include <fmt/core.h>

#include "include/capi/cef_app_capi.h"

int CEF_CALLBACK on_before_popup(
	cef_life_span_handler_t*,
	cef_browser_t*,
	cef_frame_t*,
	const cef_string_t*,
	const cef_string_t*,
	cef_window_open_disposition_t,
	int,
	const cef_popup_features_t*,
	cef_window_info_t*,
	cef_client_t**,
	cef_browser_settings_t*,
	cef_dictionary_value_t**,
	int*
);
void CEF_CALLBACK on_after_created(cef_life_span_handler_t*, cef_browser_t*);
int CEF_CALLBACK do_close(cef_life_span_handler_t*, cef_browser_t*);
void CEF_CALLBACK on_before_close(cef_life_span_handler_t*, cef_browser_t*);

namespace LifeSpanHandler {
	Browser::LifeSpanHandler* resolve(cef_client_t* handler) {
		return reinterpret_cast<Browser::LifeSpanHandler*>(reinterpret_cast<uintptr_t>(handler) - offsetof(Browser::LifeSpanHandler, cef_handler));
	}

	Browser::LifeSpanHandler* resolve_base(cef_base_ref_counted_t* base) {
		return reinterpret_cast<Browser::LifeSpanHandler*>(reinterpret_cast<uintptr_t>(base) - (offsetof(Browser::LifeSpanHandler, cef_handler) + offsetof(cef_life_span_handler_t, base)));
	}

	void CEF_CALLBACK add_ref(cef_base_ref_counted_t* base) {
		resolve_base(base)->add_ref();
	}

	int CEF_CALLBACK release(cef_base_ref_counted_t* base) {
		return resolve_base(base)->release();
	}

	int CEF_CALLBACK has_one_ref(cef_base_ref_counted_t* base) {
		return (resolve_base(base)->refcount == 1) ? 1 : 0;
	}

	int CEF_CALLBACK has_any_refs(cef_base_ref_counted_t* base) {
		return (resolve_base(base)->refcount >= 1) ? 1 : 0;
	}
}

Browser::LifeSpanHandler::LifeSpanHandler() {
	this->cef_handler.base.size = sizeof(cef_life_span_handler_t);
	this->cef_handler.base.add_ref = ::LifeSpanHandler::add_ref;
	this->cef_handler.base.release = ::LifeSpanHandler::release;
	this->cef_handler.base.has_one_ref = ::LifeSpanHandler::has_one_ref;
	this->cef_handler.base.has_at_least_one_ref = ::LifeSpanHandler::has_any_refs;
	this->cef_handler.on_before_popup = ::on_before_popup;
	this->cef_handler.on_after_created = ::on_after_created;
	this->cef_handler.do_close = ::do_close;
	this->cef_handler.on_before_close = ::on_before_close;
	this->refcount = 1;
}

void Browser::LifeSpanHandler::add_ref() {
	this->refcount += 1;
}

int Browser::LifeSpanHandler::release() {
	// Since this->refcount is atomic, the operation of decrementing it and checking if the result
	// is 0 must only access the value once. Accessing it twice would cause a race condition.
	if (--this->refcount == 0) {
		this->destroy();
		return 1;
	}
	return 0;
}

void Browser::LifeSpanHandler::destroy() {
	// Any self-cleanup should be done here
}

cef_life_span_handler_t* Browser::LifeSpanHandler::handler() {
	this->add_ref();
	return &this->cef_handler;
}

int CEF_CALLBACK on_before_popup(
	cef_life_span_handler_t* self,
	cef_browser_t* browser,
	cef_frame_t* frame,
	const cef_string_t* target_url,
	const cef_string_t* target_frame_name,
	cef_window_open_disposition_t target_disposition,
	int user_gesture,
	const cef_popup_features_t* popup_features,
	cef_window_info_t* window_info,
	cef_client_t** client,
	cef_browser_settings_t* settings,
	cef_dictionary_value_t** extra_info,
	int* no_javascript_access
) {
	fmt::print("on_before_popup\n");

	// Returning 1 would cancel creation of the popup, 0 allows it
	return 0;
}

void CEF_CALLBACK on_after_created(cef_life_span_handler_t*, cef_browser_t*) {
	fmt::print("on_after_created\n");
}

int CEF_CALLBACK do_close(cef_life_span_handler_t*, cef_browser_t*) {
	fmt::print("do_close\n");
	// Called after JS onunload function. Returning 0 here indicates we want standard behaviour,
	// returning 1 would indicate that we're going to handle the close-request manually.
	return 0;
}

void CEF_CALLBACK on_before_close(cef_life_span_handler_t*, cef_browser_t*) {
	fmt::print("on_before_close\n");
	cef_quit_message_loop();
}
