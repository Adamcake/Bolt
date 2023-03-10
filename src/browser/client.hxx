#ifndef _BOLT_CLIENT_HXX_
#define _BOLT_CLIENT_HXX_

#include <atomic>

#include "handler/life_span_handler.hxx"
#include "include/capi/cef_client_capi.h"

namespace Browser {
	struct Client {
		cef_client_t cef_client;
		std::atomic_ulong refcount;
		LifeSpanHandler* life_span_handler;
		
		Client(LifeSpanHandler*);
		void add_ref();
		int release();
		void destroy();
		cef_client_t* client();

		Client(const Client&) = delete;
		Client& operator=(const Client&) = delete;
	};
}

#endif
