#include "app.hxx"

Cef::App* resolve_app(cef_app_t* app) {
	return reinterpret_cast<Cef::App*>(reinterpret_cast<size_t>(app) - offsetof(Cef::App, cef_app));
}

Cef::App* resolve_base(cef_base_ref_counted_t* base) {
	return reinterpret_cast<Cef::App*>(reinterpret_cast<size_t>(base) - (offsetof(Cef::App, cef_app) + offsetof(cef_app_t, base)));
}

Cef::App::App() {
	this->cef_app.base.size = sizeof(cef_base_ref_counted_t);
	this->cef_app.base.add_ref = Cef::AddRef;
	this->cef_app.base.release = Cef::Release;
	this->cef_app.base.has_one_ref = Cef::HasOneRef;
	this->cef_app.base.has_at_least_one_ref = Cef::HasAnyRefs;
	this->cef_app.on_before_command_line_processing = Cef::OnBeforeCommandLineProcessing;
	this->cef_app.on_register_custom_schemes = Cef::OnRegisterCustomSchemes;
	this->cef_app.get_resource_bundle_handler = Cef::ResourceBundleHandler;
	this->cef_app.get_browser_process_handler = Cef::BrowserProcessHandler;
	this->cef_app.get_render_process_handler = Cef::RenderProcessHandler;
	this->refcount = 0;
}

void Cef::App::Destroy() {
	// Any self-cleanup should be done here
}

cef_app_t* Cef::App::app() {
	this->refcount += 1;
	return &this->cef_app;
}

void Cef::AddRef(cef_base_ref_counted_t* app) {
	resolve_base(app)->refcount += 1;
}

int Cef::Release(cef_base_ref_counted_t* app) {
	Cef::App* _app = resolve_base(app);
	_app->refcount -= 1;

	if (_app->refcount == 0) {
		_app->Destroy();
		return 1;
	}

	return 0;
}

int Cef::HasOneRef(cef_base_ref_counted_t* app) {
	return (resolve_base(app)->refcount == 1) ? 1 : 0;
}

int Cef::HasAnyRefs(cef_base_ref_counted_t* app) {
	return (resolve_base(app)->refcount >= 1) ? 1 : 0;
}

void Cef::OnBeforeCommandLineProcessing(cef_app_t*, const cef_string_t*, cef_command_line_t*) { }

void Cef::OnRegisterCustomSchemes(cef_app_t*, cef_scheme_registrar_t*) { }

cef_resource_bundle_handler_t* Cef::ResourceBundleHandler(cef_app_t*) {
	return nullptr;
}

cef_browser_process_handler_t* Cef::BrowserProcessHandler(cef_app_t*) {
	return nullptr;
}

cef_render_process_handler_t* Cef::RenderProcessHandler(cef_app_t*) {
	return nullptr;
}
