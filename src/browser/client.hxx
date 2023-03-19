#ifndef _BOLT_CLIENT_HXX_
#define _BOLT_CLIENT_HXX_

#include "handler/life_span_handler.hxx"
#include "include/cef_client.h"

namespace Browser {
	/// Implementation of CefClient. Store on the stack, but access only via CefRefPtr.
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_client.h
	struct Client: public CefClient {
		CefRefPtr<CefLifeSpanHandler> life_span_handler;

		Client(CefRefPtr<CefLifeSpanHandler>);
		CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
		
		Client(const Client&) = delete;
		Client& operator=(const Client&) = delete;
		void AddRef() const override { this->ref_count.AddRef(); }
		bool Release() const override { return this->ref_count.Release(); }
		bool HasOneRef() const override { return this->ref_count.HasOneRef(); }
		bool HasAtLeastOneRef() const override { return this->ref_count.HasAtLeastOneRef(); }
		private: CefRefCount ref_count;
	};
}

#endif
