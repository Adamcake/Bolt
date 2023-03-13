#ifndef _BOLT_WINDOW_DELEGATE_HXX_
#define _BOLT_WINDOW_DELEGATE_HXX_

#include <atomic>

#include "include/capi/views/cef_window_delegate_capi.h"
#include "include/capi/views/cef_browser_view_capi.h"
#include "details.hxx"

namespace Browser {
	struct WindowDelegate {
		cef_window_delegate_t cef_delegate;
		std::atomic_uint refcount;
		cef_browser_view_t* const browser_view;
		const Details details;

		WindowDelegate(cef_browser_view_t*, Details);
		void add_ref();
		int release();
		void destroy();
		cef_window_delegate_t* window_delegate();
		cef_view_delegate_t* view_delegate();

		WindowDelegate(const WindowDelegate&) = delete;
		WindowDelegate& operator=(const WindowDelegate&) = delete;
	};
}

#endif
