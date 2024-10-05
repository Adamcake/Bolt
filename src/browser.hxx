#ifndef _BOLT_BROWSER_HXX_
#define _BOLT_BROWSER_HXX_
#include "browser/common.hxx"

#include "include/cef_client.h"
#include "include/internal/cef_ptr.h"
#include "include/views/cef_window.h"
#include "include/views/cef_browser_view.h"

namespace Browser {
	struct Client;

	/// Represents a visible browser window on the user's screen. The window will exist either
	/// until the user closes it or Window::OnBrowserClosed() is called. In both cases,
	/// CefLifeSpanHandler::OnBeforeClose callback will be called (implemented by Client).
	///
	/// This struct also acts as the CefClient, CefWindowDelegate and CefBrowserViewDelegate and
	/// CefLifeSpanHandler for the window.
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_client.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_life_span_handler.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/views/cef_window_delegate.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/views/cef_browser_view_delegate.h
	struct Window: CefClient, CefLifeSpanHandler, CefWindowDelegate, CefBrowserViewDelegate {
		/// Calls this->Init internally
		Window(CefRefPtr<Browser::Client> client, Details, CefString, bool);

		/// Does not call this->Init internally
		Window(CefRefPtr<Browser::Client> client, Details, bool);

		/// Initialise with a browser_view. Should be called from a constructor, if at all.
		void Init(CefString);

		/// Refreshes this browser, ignoring cache. Can be called from any thread in the browser process.
		virtual void Refresh() const;

		/// Calls browser->IsSame
		bool IsSameBrowser(CefRefPtr<CefBrowser>) const;

		/// Requests focus for this browser window
		void Focus() const;

		/// Force-closes this browser and all of its children
		void Close();

		/// Closes all of this window's child windows except its devtools window, if open.
		void CloseChildrenExceptDevtools();

		/// Opens devtools for this browser
		void ShowDevTools();

		/// Called by OnBeforeClose when this browser has closed. Does nothing by default.
		virtual void NotifyClosed();

		/// Send CEF process message for this browser
		virtual void SendMessage(CefString);

		/* CefClient overrides */
		CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;

		/* CefLifeSpanHandler overrides */
		bool OnBeforePopup(
			CefRefPtr<CefBrowser>,
			CefRefPtr<CefFrame>,
			const CefString&,
			const CefString&,
			CefLifeSpanHandler::WindowOpenDisposition,
			bool,
			const CefPopupFeatures&,
			CefWindowInfo&,
			CefRefPtr<CefClient>&,
			CefBrowserSettings&,
			CefRefPtr<CefDictionaryValue>&,
			bool*
		) override;
		void OnAfterCreated(CefRefPtr<CefBrowser>) override;
		void OnBeforeClose(CefRefPtr<CefBrowser>) override;

		/* CefWindowDelegate functions */
		void OnWindowCreated(CefRefPtr<CefWindow>) override;
		void OnWindowClosing(CefRefPtr<CefWindow>) override;
		CefRect GetInitialBounds(CefRefPtr<CefWindow>) override;
		cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow>) override;
		bool IsFrameless(CefRefPtr<CefWindow>) override;
		bool CanResize(CefRefPtr<CefWindow>) override;
		bool CanMaximize(CefRefPtr<CefWindow>) override;
		bool CanMinimize(CefRefPtr<CefWindow>) override;
		bool CanClose(CefRefPtr<CefWindow>) override;
		CefSize GetPreferredSize(CefRefPtr<CefView>) override;

		/* CefBrowserViewDelegate functions */
		CefRefPtr<CefBrowserViewDelegate> GetDelegateForPopupBrowserView(CefRefPtr<CefBrowserView>, const CefBrowserSettings&, CefRefPtr<CefClient>, bool) override;
		bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView>, CefRefPtr<CefBrowserView>, bool) override;
		void OnBrowserCreated(CefRefPtr<CefBrowserView>, CefRefPtr<CefBrowser>) override;
		void OnBrowserDestroyed(CefRefPtr<CefBrowserView>, CefRefPtr<CefBrowser>) override;

		protected:
			size_t browser_count;
			bool pending_delete;
			bool show_devtools;
			Details details;
			Client* client;
			CefRefPtr<CefWindow> window;
			CefRefPtr<CefBrowserView> browser_view;
			CefRefPtr<CefBrowser> browser;
			CefRefPtr<Window> pending_child;
			std::vector<CefRefPtr<Window>> children;
			CefPopupFeatures popup_features;

			IMPLEMENT_REFCOUNTING(Window);
			DISALLOW_COPY_AND_ASSIGN(Window);
	};
}

#endif
