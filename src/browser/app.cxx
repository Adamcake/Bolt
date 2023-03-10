#include "app.hxx"

#include <fmt/core.h>

void CEF_CALLBACK add_ref(cef_base_ref_counted_t*);
int CEF_CALLBACK release(cef_base_ref_counted_t*);
int CEF_CALLBACK has_one_ref(cef_base_ref_counted_t*);
int CEF_CALLBACK has_any_refs(cef_base_ref_counted_t*);
void CEF_CALLBACK on_before_command_line_processing(cef_app_t*, const cef_string_t*, cef_command_line_t*);
void CEF_CALLBACK on_register_custom_schemes(cef_app_t*, cef_scheme_registrar_t*);
cef_resource_bundle_handler_t* CEF_CALLBACK resource_bundle_handler(cef_app_t*);
cef_browser_process_handler_t* CEF_CALLBACK browser_process_handler(cef_app_t*);
cef_render_process_handler_t* CEF_CALLBACK render_process_handler(cef_app_t*);

Browser::App* resolve_app(cef_app_t* app) {
	return reinterpret_cast<Browser::App*>(reinterpret_cast<size_t>(app) - offsetof(Browser::App, cef_app));
}

Browser::App* resolve_app_base(cef_base_ref_counted_t* base) {
	return reinterpret_cast<Browser::App*>(reinterpret_cast<size_t>(base) - (offsetof(Browser::App, cef_app) + offsetof(cef_app_t, base)));
}

Browser::App::App() {
	this->cef_app.base.size = sizeof(cef_base_ref_counted_t);
	this->cef_app.base.add_ref = ::add_ref;
	this->cef_app.base.release = ::release;
	this->cef_app.base.has_one_ref = ::has_one_ref;
	this->cef_app.base.has_at_least_one_ref = ::has_any_refs;
	this->cef_app.on_before_command_line_processing = ::on_before_command_line_processing;
	this->cef_app.on_register_custom_schemes = ::on_register_custom_schemes;
	this->cef_app.get_resource_bundle_handler = ::resource_bundle_handler;
	this->cef_app.get_browser_process_handler = ::browser_process_handler;
	this->cef_app.get_render_process_handler = ::render_process_handler;
	this->refcount = 1;
}

void Browser::App::add_ref() {
	this->refcount += 1;
}

int Browser::App::release() {
	// Since this->refcount is atomic, the operation of decrementing it and checking if the result
	// is 0 must only access the value once. Accessing it twice would cause a race condition.
	if (--this->refcount == 0) {
		this->destroy();
		return 1;
	}
	return 0;
}

void Browser::App::destroy() {
	// Any self-cleanup should be done here
}

cef_app_t* Browser::App::app() {
	this->add_ref();
	return &this->cef_app;
}

void CEF_CALLBACK add_ref(cef_base_ref_counted_t* app) {
	resolve_app_base(app)->add_ref();
}

int CEF_CALLBACK release(cef_base_ref_counted_t* app) {
	return resolve_app_base(app)->release();
}

int CEF_CALLBACK has_one_ref(cef_base_ref_counted_t* app) {
	return (resolve_app_base(app)->refcount == 1) ? 1 : 0;
}

int CEF_CALLBACK has_any_refs(cef_base_ref_counted_t* app) {
	return (resolve_app_base(app)->refcount >= 1) ? 1 : 0;
}

void CEF_CALLBACK on_before_command_line_processing(cef_app_t*, const cef_string_t*, cef_command_line_t*) {
	fmt::print("on_before_command_line_processing\n");
}

void CEF_CALLBACK on_register_custom_schemes(cef_app_t*, cef_scheme_registrar_t*) {
	fmt::print("on_register_custom_schemes\n");
}

cef_resource_bundle_handler_t* CEF_CALLBACK resource_bundle_handler(cef_app_t*) {
	fmt::print("resource_bundle_handler\n");
	return nullptr;
}

cef_browser_process_handler_t* CEF_CALLBACK browser_process_handler(cef_app_t*) {
	fmt::print("browser_process_handler\n");
	return nullptr;
}

cef_render_process_handler_t* CEF_CALLBACK render_process_handler(cef_app_t*) {
	fmt::print("render_process_handler\n");
	return nullptr;
}
