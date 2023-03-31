#ifndef _BOLT_CLIENT_HXX_
#define _BOLT_CLIENT_HXX_

#include "include/cef_client.h"

namespace Browser {
	/// Implementation of CefClient, CefLifeSpanHandler, CefRequestHandler. Store on the stack, but access only via CefRefPtr.
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_client.h
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_life_span_handler.h
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_request_handler.h
	struct Client: public CefClient, CefLifeSpanHandler, CefRequestHandler {
		Client();
		CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
		CefRefPtr<CefRequestHandler> GetRequestHandler() override;
		bool DoClose(CefRefPtr<CefBrowser>) override;
		void OnBeforeClose(CefRefPtr<CefBrowser>) override;
		CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
			CefRefPtr<CefBrowser>,
			CefRefPtr<CefFrame>,
			CefRefPtr<CefRequest>,
			bool,
			bool,
			const CefString&,
			bool&
		) override;
		
		Client(const Client&) = delete;
		Client& operator=(const Client&) = delete;
		void AddRef() const override { this->ref_count.AddRef(); }
		bool Release() const override { return this->ref_count.Release(); }
		bool HasOneRef() const override { return this->ref_count.HasOneRef(); }
		bool HasAtLeastOneRef() const override { return this->ref_count.HasAtLeastOneRef(); }
		private:
			CefRefCount ref_count;
			CefString app_overlay_url;
	};
}

#endif
