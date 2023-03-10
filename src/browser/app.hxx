#ifndef _BOLT_APP_HXX_
#define _BOLT_APP_HXX_

#include <atomic>

#include "include/capi/cef_app_capi.h"

namespace Browser {
	struct App {
		cef_app_t cef_app;
		std::atomic_ulong refcount;
		
		App();
		void add_ref();
		int release();
		void destroy();
		cef_app_t* app();

		App(const App&) = delete;
		App& operator=(const App&) = delete;
	};
}

#endif
