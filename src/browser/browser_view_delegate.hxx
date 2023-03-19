#ifndef _BOLT_BROWSER_VIEW_DELEGATE_HXX_
#define _BOLT_BROWSER_VIEW_DELEGATE_HXX_

#include "include/internal/cef_types.h"
#include "include/views/cef_browser_view_delegate.h"
#include "details.hxx"

namespace Browser {
	struct BrowserViewDelegate: public CefBrowserViewDelegate {
		const Details details;

		BrowserViewDelegate(Details);
		CefSize GetPreferredSize(CefRefPtr<CefView>) override;
		CefSize GetMinimumSize(CefRefPtr<CefView>) override;
		CefSize GetMaximumSize(CefRefPtr<CefView>) override;
		//int GetHeightForWidth(CefRefPtr<CefView>, int) override;
		cef_chrome_toolbar_type_t GetChromeToolbarType() override;

		private:
			IMPLEMENT_REFCOUNTING(BrowserViewDelegate);
			DISALLOW_COPY_AND_ASSIGN(BrowserViewDelegate);
	};
}

#endif
