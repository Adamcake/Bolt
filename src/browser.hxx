#ifndef _BOLT_BROWSER_HXX_
#define _BOLT_BROWSER_HXX_
#include "browser/common.hxx"
#include "include/cef_client.h"
#include "include/views/cef_window.h"
#include "include/views/cef_browser_view_delegate.h"

namespace Browser {
	/// Represents a visible browser window on the user's screen. This struct wraps a single pointer,
	/// so it is safe to store anywhere and move around during operation.
	/// The window will exist either until the user closes it or Window::CloseBrowser() is called.
	/// In both cases, CefLifeSpanHandler::OnBeforeClose callback will be called (implemented by Client).
	/// This struct also acts as the CefWindowDelegate and CefBrowserViewDelegate for itself and any
	/// children, such as devtools windows or browsers opened by Bolt apps.
	/// https://github.com/chromiumembedded/cef/blob/5563/include/views/cef_window_delegate.h
	/// https://github.com/chromiumembedded/cef/blob/5563/include/views/cef_browser_view_delegate.h
	struct Window: CefWindowDelegate, CefBrowserViewDelegate {
		Window(CefRefPtr<CefClient> client, Details, CefString);

		/// A request to close a browser can originate either from the Render process or UI process.
		/// In Bolt, if it originates in the Browser process (where this struct is), then it will be
		/// sent to the Render process to begin there.
		/// When the render process has finished deleting, (which may or may not have happened due to
		/// a call to CloseRender(), it will send a message back to the Browser process indicating it's
		/// now okay to call CloseBrowser(). CloseBrowser should not be called before that.
		void CloseRender();

		/// Should be called only in response to a 'closed' message from the Render process.
		/// Also indicates that this struct is about to be dropped and should no longer be used.
		void CloseBrowser();

		/// Calls GetIdentifier() on the internal CefBrowser
		int GetBrowserIdentifier() const;

		/// Opens dev tools for this window by calling CefBrowser::ShowDevTools with some standard settings
		void ShowDevTools(CefRefPtr<CefClient>);

		/// Checks whether CloseBrowser() has been called and if so, whether the unique ID of its browser
		/// matches the given param.
		bool IsClosingWithHandle(int) const;

		/* CefWindowDelegate functions */
		void OnWindowCreated(CefRefPtr<CefWindow>) override;
		void OnWindowDestroyed(CefRefPtr<CefWindow>) override;
		CefRect GetInitialBounds(CefRefPtr<CefWindow>) override;
		cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow>) override;
		bool IsFrameless(CefRefPtr<CefWindow>) override;
		bool CanResize(CefRefPtr<CefWindow>) override;
		bool CanMaximize(CefRefPtr<CefWindow>) override;
		bool CanMinimize(CefRefPtr<CefWindow>) override;
		bool CanClose(CefRefPtr<CefWindow>) override;
		CefSize GetPreferredSize(CefRefPtr<CefView>) override;
		CefSize GetMinimumSize(CefRefPtr<CefView>) override;
		CefSize GetMaximumSize(CefRefPtr<CefView>) override;

		/* CefBrowserViewDelegate functions */
		cef_chrome_toolbar_type_t GetChromeToolbarType() override;

		private:
			bool closing;
			int closing_handle;

			Details details;
			CefRefPtr<CefWindow> window;
			CefRefPtr<CefBrowserView> browser_view;

			IMPLEMENT_REFCOUNTING(Window);
			DISALLOW_COPY_AND_ASSIGN(Window);
	};
}

#endif
