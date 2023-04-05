#ifndef _BOLT_NATIVE_HXX_
#define _BOLT_NATIVE_HXX_
#include "include/views/cef_window.h"

#ifdef __linux__
#include <xcb/xcb.h>
#endif

namespace Native {
	/// Handle to a CEF window. Concrete type depends on what backend CEF is using.
	typedef CefWindowHandle BrowserHandle;

	/// Struct representing a connection to the OS (X11, Win32, etc)
	struct Connection {
		#ifdef __linux__
		xcb_connection_t* connection;
		#endif

		#ifdef _WIN32
		// todo
		#endif

		#ifdef __APPLE__
		// todo
		#endif
	};
}

#endif
