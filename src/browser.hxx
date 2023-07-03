#ifndef _BOLT_BROWSER_HXX_
#define _BOLT_BROWSER_HXX_
#include "browser/common.hxx"
#include "include/cef_client.h"
#include "include/views/cef_window.h"
#include "include/views/cef_browser_view.h"

namespace Browser {
	/// Represents a visible browser window on the user's screen. This struct wraps a single pointer,
	/// so it is safe to store anywhere and move around during operation.
	/// The window will exist either until the user closes it or Window::CloseBrowser() is called.
	/// In both cases, CefLifeSpanHandler::OnBeforeClose callback will be called (implemented by Client).
	/// This struct also acts as the CefWindowDelegate and CefBrowserViewDelegate for itself and any
	/// children, such as devtools windows or browsers opened by Bolt apps.
	/// https://github.com/chromiumembedded/cef/blob/5735/include/views/cef_window_delegate.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/views/cef_browser_view_delegate.h
	struct Window: CefWindowDelegate, CefBrowserViewDelegate {
		Window(CefRefPtr<CefClient> client, Details, CefString);
		Window(Details);

		/// Purges matching windows from this window's list of children
		/// Returns true if this window itself matches the given browser, false otherwise
		/// Cannot be called from CefLifeSpanHandler::OnBeforeClose, see CefBrowserViewDelegate::OnBrowserDestroyed for info
		bool CloseBrowser(CefRefPtr<CefBrowser>);

		/// Assigns given popup features to to matching windows
		/// Given popup features will be used for the next child window to open
		void SetPopupFeaturesForBrowser(CefRefPtr<CefBrowser>, const CefPopupFeatures&);

		/* CefWindowDelegate functions */
		void OnWindowCreated(CefRefPtr<CefWindow>) override;
		void OnWindowClosing(CefRefPtr<CefWindow>) override;
		CefRect GetInitialBounds(CefRefPtr<CefWindow>) override;
		cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow>) override;
		bool IsFrameless(CefRefPtr<CefWindow>) override;
		bool CanResize(CefRefPtr<CefWindow>) override;
		bool CanMaximize(CefRefPtr<CefWindow>) override;
		bool CanMinimize(CefRefPtr<CefWindow>) override;
		bool CanClose(CefRefPtr<CefWindow>) override;
		CefSize GetPreferredSize(CefRefPtr<CefView>) override;

		/* CefBrowserViewDelegate functions */
		CefRefPtr<CefBrowserViewDelegate> GetDelegateForPopupBrowserView(CefRefPtr<CefBrowserView>, const CefBrowserSettings&, CefRefPtr<CefClient>, bool) override;
		bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView>, CefRefPtr<CefBrowserView>, bool) override;
		void OnBrowserCreated(CefRefPtr<CefBrowserView>, CefRefPtr<CefBrowser>) override;
		cef_chrome_toolbar_type_t GetChromeToolbarType() override;

		private:
			Details details;
			CefRefPtr<CefWindow> window;
			CefRefPtr<CefBrowserView> browser_view;
			CefRefPtr<Window> pending_child;
			std::vector<CefRefPtr<Window>> children;
			CefPopupFeatures popup_features;

			IMPLEMENT_REFCOUNTING(Window);
			DISALLOW_COPY_AND_ASSIGN(Window);
	};
}

#endif
