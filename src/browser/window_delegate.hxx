#ifndef _BOLT_WINDOW_DELEGATE_HXX_
#define _BOLT_WINDOW_DELEGATE_HXX_

#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "include/views/cef_browser_view_delegate.h"
#include "common.hxx"

namespace Browser {
	/// Implementation of CefWindowDelegate. Create on the heap as CefRefPtr.
	/// https://github.com/chromiumembedded/cef/blob/5563/include/views/cef_window_delegate.h
	struct WindowDelegate: public CefWindowDelegate {
		const Details details;

		WindowDelegate(CefRefPtr<CefBrowserView>, CefRefPtr<CefBrowserViewDelegate>, Details);
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

		void Close();
		int GetBrowserIdentifier();
		void ShowDevTools(CefRefPtr<CefClient>);
		void SendProcessMessage(CefProcessId, CefRefPtr<CefProcessMessage>);

		private:
			CefRefPtr<CefWindow> window;
			CefRefPtr<CefBrowserView> browser_view;
			CefRefPtr<CefBrowserViewDelegate> browser_view_delegate;
			bool closing;

			IMPLEMENT_REFCOUNTING(WindowDelegate);
			DISALLOW_COPY_AND_ASSIGN(WindowDelegate);
	};
}

#endif
