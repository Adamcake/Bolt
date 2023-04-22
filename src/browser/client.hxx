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

		/* Functions implemented in native */

		/// Runs a platform-native event loop, blocking until it exits. Called in its own dedicated thread.
		/// This function assumes SetupNative has been called and that its return value indicated success.
		void Run();

		/// Closes the Native::Connection owned by this struct. Called on the main thread only, so should fire an
		/// event to the event loop indicating to stop. This function must be non-blocking.
		/// This function will not be called until all owned browsers have been destroyed (i.e. OnBeforeClose
		/// has been called for all Browsers), and this will be the last function call made to the Client.
		void CloseNative();
		
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
