#ifndef _BOLT_APP_HXX_
#define _BOLT_APP_HXX_

#include "include/cef_app.h"

namespace Browser {
	struct App: public CefApp {
		App();

		private:
			IMPLEMENT_REFCOUNTING(App);
			DISALLOW_COPY_AND_ASSIGN(App);
	};
}

#endif
