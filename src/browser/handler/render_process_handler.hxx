#ifndef _BOLT_RENDER_PROCESS_HANDLER_HXX_
#define _BOLT_RENDER_PROCESS_HANDLER_HXX_

#include <map>

#include "include/cef_browser.h"
#include "include/cef_load_handler.h"
#include "include/cef_render_process_handler.h"

namespace Browser {
	/// Implementation of CefRenderProcessHandler and CefLoadHandler. Store on the stack, but access only via CefRefPtr.
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_render_process_handler.h
	/// https://github.com/chromiumembedded/cef/blob/master/include/cef_load_handler.h
	struct RenderProcessHandler: public CefRenderProcessHandler, CefLoadHandler {
		RenderProcessHandler();

		void OnBrowserCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefDictionaryValue>) override;
		void OnBrowserDestroyed(CefRefPtr<CefBrowser>) override;
		void OnUncaughtException(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefV8Context>, CefRefPtr<CefV8Exception>, CefRefPtr<CefV8StackTrace>) override;
		CefRefPtr<CefLoadHandler> GetLoadHandler() override;
		void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int) override;

		RenderProcessHandler(const RenderProcessHandler&) = delete;
		RenderProcessHandler& operator=(const RenderProcessHandler&) = delete;
		void AddRef() const override { this->ref_count.AddRef(); }
		bool Release() const override { return this->ref_count.Release(); }
		bool HasOneRef() const override { return this->ref_count.HasOneRef(); }
		bool HasAtLeastOneRef() const override { return this->ref_count.HasAtLeastOneRef(); }
		private:
			std::map<int, CefString> pending_app_frames;
			CefRefCount ref_count;
	};
}

#endif
