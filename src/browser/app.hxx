#ifndef _BOLT_APP_HXX_
#define _BOLT_APP_HXX_

#include "include/cef_app.h"

namespace Browser {
	/// Implementation of CefApp. Store on the stack, but access only via CefRefPtr.
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_app.h
	struct App: public CefApp {
		App();

		App(const App&) = delete;
		App& operator=(const App&) = delete;
		void AddRef() const override { this->ref_count.AddRef(); }
		bool Release() const override { return this->ref_count.Release(); }
		bool HasOneRef() const override { return this->ref_count.HasOneRef(); }
		bool HasAtLeastOneRef() const override { return this->ref_count.HasAtLeastOneRef(); }
		private: CefRefCount ref_count;
	};
}

#endif
