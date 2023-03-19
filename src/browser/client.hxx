#ifndef _BOLT_CLIENT_HXX_
#define _BOLT_CLIENT_HXX_

#include "handler/life_span_handler.hxx"
#include "include/cef_client.h"

namespace Browser {
	struct Client: public CefClient {
		CefRefPtr<CefLifeSpanHandler> life_span_handler;

		Client(CefRefPtr<CefLifeSpanHandler>);
		CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
		
		private:
			IMPLEMENT_REFCOUNTING(Client);
			DISALLOW_COPY_AND_ASSIGN(Client);
	};
}

#endif
