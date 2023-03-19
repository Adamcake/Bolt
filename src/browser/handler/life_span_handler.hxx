#ifndef _BOLT_LIFE_SPAN_HANDLER_HXX_
#define _BOLT_LIFE_SPAN_HANDLER_HXX_

#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"

namespace Browser {
	/// Implementation of CefLifeSpanHandler. Store on the stack, but access only via CefRefPtr.
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_life_span_handler.h
	struct LifeSpanHandler: public CefLifeSpanHandler {
		LifeSpanHandler();

		void OnBeforeClose(CefRefPtr<CefBrowser>) override;

		LifeSpanHandler(const LifeSpanHandler&) = delete;
		LifeSpanHandler& operator=(const LifeSpanHandler&) = delete;
		void AddRef() const override { this->ref_count.AddRef(); }
		bool Release() const override { return this->ref_count.Release(); }
		bool HasOneRef() const override { return this->ref_count.HasOneRef(); }
		bool HasAtLeastOneRef() const override { return this->ref_count.HasAtLeastOneRef(); }
		private: CefRefCount ref_count;
	};
}

#endif
