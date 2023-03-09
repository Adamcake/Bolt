#ifndef _BOLT_APP_HXX_
#define _BOLT_APP_HXX_

#include <atomic>

#include "include/capi/cef_app_capi.h"

namespace Browser {
	struct App {
		cef_app_t cef_app;
		std::atomic_ulong refcount;
		
		App();
		void AddRef();
		int Release();
		void Destroy();
		cef_app_t* app();

		private:
			App(const App&) = delete;
			App& operator=(const App&) = delete;
	};

	void AddRef(cef_base_ref_counted_t*);
	int Release(cef_base_ref_counted_t*);
	int HasOneRef(cef_base_ref_counted_t*);
	int HasAnyRefs(cef_base_ref_counted_t*);

	void OnBeforeCommandLineProcessing(cef_app_t*, const cef_string_t*, cef_command_line_t*);
	void OnRegisterCustomSchemes(cef_app_t*, cef_scheme_registrar_t*);
	cef_resource_bundle_handler_t* ResourceBundleHandler(cef_app_t*);
	cef_browser_process_handler_t* BrowserProcessHandler(cef_app_t*);
	cef_render_process_handler_t* RenderProcessHandler(cef_app_t*);
}

#endif
