#ifndef _BOLT_RENDER_PROCESS_HANDLER_HXX_
#define _BOLT_RENDER_PROCESS_HANDLER_HXX_

#include <vector>

#include "../common.hxx"
#include "include/cef_render_process_handler.h"

namespace Browser {
	/// Implementation of CefRenderProcessHandler. Store on the stack, but access only via CefRefPtr.
	/// https://github.com/chromiumembedded/cef/blob/5563/include/cef_render_process_handler.h
	struct RenderProcessHandler: public CefRenderProcessHandler {
		RenderProcessHandler();

		void OnBrowserCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefDictionaryValue>) override;
		void OnBrowserDestroyed(CefRefPtr<CefBrowser>) override;
		void OnContextCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefV8Context>) override;
		void OnContextReleased(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefV8Context>) override;
		void OnUncaughtException(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefV8Context>, CefRefPtr<CefV8Exception>, CefRefPtr<CefV8StackTrace>) override;

		RenderProcessHandler(const RenderProcessHandler&) = delete;
		RenderProcessHandler& operator=(const RenderProcessHandler&) = delete;
		void AddRef() const override { this->ref_count.AddRef(); }
		bool Release() const override { return this->ref_count.Release(); }
		bool HasOneRef() const override { return this->ref_count.HasOneRef(); }
		bool HasAtLeastOneRef() const override { return this->ref_count.HasAtLeastOneRef(); }
		private: CefRefCount ref_count;
	};
}

#endif
