#ifndef _BOLT_CLIENT_HXX_
#define _BOLT_CLIENT_HXX_

#include <atomic>

#include "include/capi/cef_client_capi.h"

namespace Browser {
    struct Client {
        cef_client_t cef_client;
		std::atomic_ulong refcount;
        cef_life_span_handler_t* life_span_handler;
		
		Client(cef_life_span_handler_t*);
		void add_ref();
		int release();
		void destroy();
		cef_client_t* client();

		Client(const Client&) = delete;
		Client& operator=(const Client&) = delete;
    };
}

#endif
