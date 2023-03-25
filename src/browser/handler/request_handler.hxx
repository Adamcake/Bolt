#ifndef _BOLT_REQUEST_HANDLER_HXX_
#define _BOLT_REQUEST_HANDLER_HXX_

#include "include/cef_request_handler.h"

namespace Browser {
	/// Implementation of CefRequestHandler and CefResourceRequestHandler. Store on the stack, but access only via CefRefPtr.
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_request_handler.h
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_resource_request_handler.h
	struct RequestHandler: public CefRequestHandler, public CefResourceRequestHandler {
		RequestHandler();

		CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
			CefRefPtr<CefBrowser>,
			CefRefPtr<CefFrame>,
			CefRefPtr<CefRequest>,
			bool,
			bool,
			const CefString&,
			bool&
		) override;

		RequestHandler(const RequestHandler&) = delete;
		RequestHandler& operator=(const RequestHandler&) = delete;
		void AddRef() const override { this->ref_count.AddRef(); }
		bool Release() const override { return this->ref_count.Release(); }
		bool HasOneRef() const override { return this->ref_count.HasOneRef(); }
		bool HasAtLeastOneRef() const override { return this->ref_count.HasAtLeastOneRef(); }
		private: CefRefCount ref_count;
	};
}

#endif
