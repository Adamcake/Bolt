#ifndef _BOLT_APP_FRAME_DATA_HXX_
#define _BOLT_APP_FRAME_DATA_HXX_
#include "include/cef_v8.h"

namespace Browser {
	struct AppFrameData: public CefV8Handler {
		AppFrameData(int id, CefString url): id(id), url(std::move(url)), frame(nullptr) { }
		bool Execute(const CefString&, CefRefPtr<CefV8Value>, const CefV8ValueList&, CefRefPtr<CefV8Value>&, CefString&) override;

		int id;
		CefString url;
		CefRefPtr<CefFrame> frame;

		private:
			DISALLOW_COPY_AND_ASSIGN(AppFrameData);
			IMPLEMENT_REFCOUNTING(AppFrameData);
	};
}

#endif
