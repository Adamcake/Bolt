#ifndef _BOLT_DOM_VISITOR_HXX_
#define _BOLT_DOM_VISITOR_HXX_

#include "include/cef_dom.h"

// Implementations of CefDOMVisitor. Store on the heap.
// https://github.com/chromiumembedded/cef/blob/5563/include/cef_dom.h
namespace Browser {
	/// Implementation of CefDOMVisitor for loading an app's URL into the overlay's iframe.
	struct DOMVisitorAppFrame: public CefDOMVisitor {
		DOMVisitorAppFrame(const CefString);
		void Visit(CefRefPtr<CefDOMDocument>) override;

		private:
			const CefString app_url;
			DISALLOW_COPY_AND_ASSIGN(DOMVisitorAppFrame);
			IMPLEMENT_REFCOUNTING(DOMVisitorAppFrame);
	};
}

#endif
