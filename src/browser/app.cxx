#include "app.hxx"

Browser::App::App(CefRefPtr<CefRenderProcessHandler> render_process_handler): render_process_handler(render_process_handler) {
	
}

CefRefPtr<CefRenderProcessHandler> Browser::App::GetRenderProcessHandler() {
	return this->render_process_handler;
}
