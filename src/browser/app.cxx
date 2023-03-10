#include "app.hxx"

#include <fmt/core.h>

void AddRef(cef_base_ref_counted_t*);
int Release(cef_base_ref_counted_t*);
int HasOneRef(cef_base_ref_counted_t*);
int HasAnyRefs(cef_base_ref_counted_t*);
void OnBeforeCommandLineProcessing(cef_app_t*, const cef_string_t*, cef_command_line_t*);
void OnRegisterCustomSchemes(cef_app_t*, cef_scheme_registrar_t*);
cef_resource_bundle_handler_t* ResourceBundleHandler(cef_app_t*);
cef_browser_process_handler_t* BrowserProcessHandler(cef_app_t*);
cef_render_process_handler_t* RenderProcessHandler(cef_app_t*);

Browser::App* resolve_app(cef_app_t* app) {
	return reinterpret_cast<Browser::App*>(reinterpret_cast<size_t>(app) - offsetof(Browser::App, cef_app));
}

Browser::App* resolve_base(cef_base_ref_counted_t* base) {
	return reinterpret_cast<Browser::App*>(reinterpret_cast<size_t>(base) - (offsetof(Browser::App, cef_app) + offsetof(cef_app_t, base)));
}

Browser::App::App() {
	this->cef_app.base.size = sizeof(cef_base_ref_counted_t);
	this->cef_app.base.add_ref = ::AddRef;
	this->cef_app.base.release = ::Release;
	this->cef_app.base.has_one_ref = ::HasOneRef;
	this->cef_app.base.has_at_least_one_ref = ::HasAnyRefs;
	this->cef_app.on_before_command_line_processing = ::OnBeforeCommandLineProcessing;
	this->cef_app.on_register_custom_schemes = ::OnRegisterCustomSchemes;
	this->cef_app.get_resource_bundle_handler = ::ResourceBundleHandler;
	this->cef_app.get_browser_process_handler = ::BrowserProcessHandler;
	this->cef_app.get_render_process_handler = ::RenderProcessHandler;
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

void CEF_CALLBACK AddRef(cef_base_ref_counted_t* app) {
	resolve_base(app)->add_ref();
}

int CEF_CALLBACK Release(cef_base_ref_counted_t* app) {
	return resolve_base(app)->release();
}

int CEF_CALLBACK HasOneRef(cef_base_ref_counted_t* app) {
	return (resolve_base(app)->refcount == 1) ? 1 : 0;
}

int CEF_CALLBACK HasAnyRefs(cef_base_ref_counted_t* app) {
	return (resolve_base(app)->refcount >= 1) ? 1 : 0;
}

void CEF_CALLBACK OnBeforeCommandLineProcessing(cef_app_t*, const cef_string_t*, cef_command_line_t*) {
	fmt::print("OnBeforeCommandLineProcessing\n");
}

void CEF_CALLBACK OnRegisterCustomSchemes(cef_app_t*, cef_scheme_registrar_t*) {
	fmt::print("OnRegisterCustomSchemes\n");
}

cef_resource_bundle_handler_t* CEF_CALLBACK ResourceBundleHandler(cef_app_t*) {
	fmt::print("ResourceBundleHandler\n");
	return nullptr;
}

cef_browser_process_handler_t* CEF_CALLBACK BrowserProcessHandler(cef_app_t*) {
	fmt::print("BrowserProcessHandler\n");
	return nullptr;
}

cef_render_process_handler_t* CEF_CALLBACK RenderProcessHandler(cef_app_t*) {
	fmt::print("RenderProcessHandler\n");
	return nullptr;
}
