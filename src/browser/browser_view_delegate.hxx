#ifndef _BOLT_BROWSER_VIEW_DELEGATE_HXX_
#define _BOLT_BROWSER_VIEW_DELEGATE_HXX_

#include <atomic>

#include "include/capi/views/cef_browser_view_delegate_capi.h"
#include "details.hxx"

namespace Browser {
	struct BrowserViewDelegate {
		cef_browser_view_delegate_t cef_delegate;
		std::atomic_ulong refcount;
		const Details details;

		BrowserViewDelegate(Details);
		void add_ref();
		int release();
		void destroy();
		cef_browser_view_delegate_t* delegate();

		BrowserViewDelegate(const BrowserViewDelegate&) = delete;
		BrowserViewDelegate& operator=(const BrowserViewDelegate&) = delete;
	};
}

#endif
