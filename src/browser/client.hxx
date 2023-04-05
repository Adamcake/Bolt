#ifndef _BOLT_CLIENT_HXX_
#define _BOLT_CLIENT_HXX_
#include "include/cef_client.h"
#include "include/views/cef_window_delegate.h"
#include "app.hxx"
#include "../browser.hxx"
#include "../native/native.hxx"

#include <vector>
#include <mutex>

namespace Browser {
	/// Implementation of CefClient, CefBrowserProcessHandler, CefLifeSpanHandler, CefRequestHandler.
	/// Store on the stack, but access only via CefRefPtr.
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_client.h
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_browser_process_handler.h
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_life_span_handler.h
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_request_handler.h
	struct Client: public CefClient, CefBrowserProcessHandler, CefLifeSpanHandler, CefRequestHandler {
		Client(CefRefPtr<Browser::App>);

		/* CefClient overrides */
		CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
		CefRefPtr<CefRequestHandler> GetRequestHandler() override;
		bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefProcessId, CefRefPtr<CefProcessMessage>) override;

		/* CefBrowserProcessHandler overrides */
		void OnContextInitialized() override;
		void OnScheduleMessagePumpWork(int64) override;

		/* CefLifeSpanHandler overrides */
		bool DoClose(CefRefPtr<CefBrowser>) override;
		void OnBeforeClose(CefRefPtr<CefBrowser>) override;

		/* CefRequestHandler overrides */
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

			Native::Native native;

			// Mutex-locked vector - may be accessed from either UI thread (most of the time) or IO thread (GetResourceRequestHandler)
			std::vector<CefRefPtr<Browser::Window>> apps;
			std::mutex apps_lock;
	};
}

#endif
