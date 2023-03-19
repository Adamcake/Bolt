#ifndef _BOLT_LIFE_SPAN_HANDLER_HXX_
#define _BOLT_LIFE_SPAN_HANDLER_HXX_

#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"

namespace Browser {
	struct LifeSpanHandler: public CefLifeSpanHandler {
		LifeSpanHandler();

		void OnBeforeClose(CefRefPtr<CefBrowser>) override;

		private:
			IMPLEMENT_REFCOUNTING(LifeSpanHandler);
			DISALLOW_COPY_AND_ASSIGN(LifeSpanHandler);
	};
}

#endif
