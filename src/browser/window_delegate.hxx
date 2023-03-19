#ifndef _BOLT_WINDOW_DELEGATE_HXX_
#define _BOLT_WINDOW_DELEGATE_HXX_

#include "include/views/cef_window_delegate.h"
#include "include/views/cef_browser_view.h"
#include "details.hxx"

namespace Browser {
	struct WindowDelegate: public CefWindowDelegate {
		const CefRefPtr<CefBrowserView> browser_view;
		const Details details;

		WindowDelegate(CefRefPtr<CefBrowserView>, Details);
		void OnWindowCreated(CefRefPtr<CefWindow>) override;
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

		private:
			IMPLEMENT_REFCOUNTING(WindowDelegate);
			DISALLOW_COPY_AND_ASSIGN(WindowDelegate);
	};
}

#endif
