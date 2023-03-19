#ifndef _BOLT_BROWSER_HXX_
#define _BOLT_BROWSER_HXX_

#include "browser/details.hxx"
#include "browser/window_delegate.hxx"

namespace Browser {
	typedef CefRefPtr<WindowDelegate> Window;

	/// Creates and shows browser window with the given parameters.
	/// The window will exist until the user closes it or the returned object is destroyed.
	Window Create(CefRefPtr<CefClient> client, Details);
}

#endif
