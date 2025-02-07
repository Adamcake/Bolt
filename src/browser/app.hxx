#ifndef _BOLT_APP_HXX_
#define _BOLT_APP_HXX_

#include "include/cef_app.h"
#include "include/cef_base.h"
#include "include/cef_browser_process_handler.h"
#include <vector>

namespace Browser {
	/// Implementation of CefApp, CefRenderProcessHandler, CefLoadHandler, CefV8Handler. Store on the stack, but access only via CefRefPtr.
	/// https://github.com/chromiumembedded/cef/blob/6478/include/cef_app.h
	/// https://github.com/chromiumembedded/cef/blob/6478/include/cef_render_process_handler.h
	/// https://github.com/chromiumembedded/cef/blob/6478/include/cef_load_handler.h
	/// https://github.com/chromiumembedded/cef/blob/6478/include/cef_v8.h#L230
	struct App: public CefApp, CefRenderProcessHandler, CefLoadHandler, CefV8Handler {
		App();
		void SetBrowserProcessHandler(CefRefPtr<CefBrowserProcessHandler> handler);
		CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;
		CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
		CefRefPtr<CefLoadHandler> GetLoadHandler() override;
		void OnBrowserCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefDictionaryValue>) override;
		void OnContextCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefV8Context>) override;
		void OnUncaughtException(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefV8Context>, CefRefPtr<CefV8Exception>, CefRefPtr<CefV8StackTrace>) override;
		bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefProcessId, CefRefPtr<CefProcessMessage>) override;
		void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int) override;
		void OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, ErrorCode, const CefString&, const CefString&) override;
		bool Execute(const CefString&, CefRefPtr<CefV8Value>, const CefV8ValueList&, CefRefPtr<CefV8Value>&, CefString&) override;
		void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar>) override;

		App(const App&) = delete;
		App& operator=(const App&) = delete;
		void AddRef() const override { this->ref_count.AddRef(); }
		bool Release() const override { return this->ref_count.Release(); }
		bool HasOneRef() const override { return this->ref_count.HasOneRef(); }
		bool HasAtLeastOneRef() const override { return this->ref_count.HasAtLeastOneRef(); }
		private:
			CefRefCount ref_count;
			CefRefPtr<CefBrowserProcessHandler> browser_process_handler;
			std::vector<CefRefPtr<CefProcessMessage>> pending_plugin_messages;
			bool loaded;
	};
}

#endif
