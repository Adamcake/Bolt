#ifndef _BOLT_LIFE_SPAN_HANDLER_HXX_
#define _BOLT_LIFE_SPAN_HANDLER_HXX_
#include <atomic>

#include "include/capi/cef_client_capi.h"

namespace Browser {
	struct LifeSpanHandler {
		cef_life_span_handler_t cef_handler;
		std::atomic_ulong refcount;
		
		LifeSpanHandler();
		void add_ref();
		int release();
		void destroy();
		cef_life_span_handler_t* handler();

		LifeSpanHandler(const LifeSpanHandler&) = delete;
		LifeSpanHandler& operator=(const LifeSpanHandler&) = delete;
	};
}

#endif
