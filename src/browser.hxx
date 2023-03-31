#ifndef _BOLT_BROWSER_HXX_
#define _BOLT_BROWSER_HXX_

#include "browser/common.hxx"
#include "browser/window_delegate.hxx"

namespace Browser {
	/// Represents a visible browser window on the user's screen. This struct wraps a single pointer,
	/// so it is safe to store anywhere and move around during operation.
	/// The window will exist either until the user closes it or Window::CloseBrowser() is called.
	/// In both cases, CefLifeSpanHandler::OnBeforeClose callback will be called (implemented by Client).
	struct Window {
		Window(CefRefPtr<CefClient> client, Details);
		/// A request to close a browser can originate either from the Render process or UI process.
		/// In Bolt, if it originates in the Browser process (where this struct is), then it will be
		/// sent to the Render process to begin there.
		/// When the render process has finished deleting, (which may or may not have happened due to
		/// a call to CloseRender(), it will send a message back to the Browser process indicating it's
		/// now okay to call CloseBrowser(). CloseBrowser should not be called before that.
		void CloseRender();

		/// Should be called only in response to a 'closed' message from the Render process.
		void CloseBrowser();

		/// Calls GetIdentifier() on the internal CefBrowser
		int GetBrowserIdentifier();

		CefRefPtr<WindowDelegate> window_delegate;
	};
}

#endif
