#ifndef _BOLT_BROWSER_HXX_
#define _BOLT_BROWSER_HXX_
#include "browser/common.hxx"
#include "file_manager.hxx"

#include "include/cef_client.h"
#include "include/internal/cef_ptr.h"
#include "include/views/cef_window.h"
#include "include/views/cef_browser_view.h"

namespace Browser {
	struct Client;

	/// Represents a visible browser window on the user's screen. This struct wraps a single pointer,
	/// so it is safe to store anywhere and move around during operation.
	/// The window will exist either until the user closes it or Window::OnBrowserClosed() is called.
	/// In both cases, CefLifeSpanHandler::OnBeforeClose callback will be called (implemented by Client).
	/// This struct also acts as the CefWindowDelegate and CefBrowserViewDelegate for the window it represents,
	/// and the CefResourceRequestHandler for requests originating from this window or any of its children.
	/// https://github.com/chromiumembedded/cef/blob/5735/include/views/cef_window_delegate.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/views/cef_browser_view_delegate.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_resource_request_handler.h
	struct Window: CefWindowDelegate, CefBrowserViewDelegate, CefRequestHandler {
		/// Calls this->Init internally
		Window(CefRefPtr<Browser::Client> client, Details, CefString, bool);

		/// Does not call this->Init internally
		Window(CefRefPtr<Browser::Client>, Details, bool);

		/// Returns true if this window is a launcher, false otherwise.
		/// Used to verify that only one launcher may be open at a time.
		virtual bool IsLauncher() const;

		/// Initialise with a browser_view. Should be called from a constructor, if at all.
		void Init(CefRefPtr<CefClient> client, Details, CefString, bool);

		/// Refreshes this browser, ignoring cache. Can be called from any thread in the browser process.
		virtual void Refresh() const;

		/// Returns true if the given browser is this window or one of its children, otherwise false
		bool HasBrowser(CefRefPtr<CefBrowser>) const;

		/// Counts how many browsers this browser is responsible for, including itself and children recursively
		size_t CountBrowsers() const;

		/// Requests focus for this browser window
		void Focus() const;

		/// Force-closes this browser and all of its children
		void Close();

		/// Closes all of this window's child windows except its devtools window, if open.
		void CloseChildrenExceptDevtools();

		/// Purges matching windows from this window's list of children
		/// Returns true if this window itself matches the given browser, false otherwise
		/// Cannot be called from CefLifeSpanHandler::OnBeforeClose, see CefBrowserViewDelegate::OnBrowserDestroyed for info
		bool OnBrowserClosing(CefRefPtr<CefBrowser>);

		/// Assigns given popup features to to matching windows
		/// Given popup features will be used for the next child window to open
		void SetPopupFeaturesForBrowser(CefRefPtr<CefBrowser>, const CefPopupFeatures&);

		/// Opens devtools for this browser
		void ShowDevTools();

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

		/* CefRequestHandler functions */
		CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
			CefRefPtr<CefBrowser>,
			CefRefPtr<CefFrame>,
			CefRefPtr<CefRequest>,
			bool,
			bool,
			const CefString&,
			bool&
		) override;

		protected:
			bool show_devtools;
			Details details;
			Client* client;
			CefRefPtr<CefWindow> window;
			CefRefPtr<CefBrowserView> browser_view;
			CefRefPtr<CefBrowser> browser;
			CefRefPtr<FileManager::FileManager> file_manager;
			CefRefPtr<Window> pending_child;
			std::vector<CefRefPtr<Window>> children;
			CefPopupFeatures popup_features;

			IMPLEMENT_REFCOUNTING(Window);
			DISALLOW_COPY_AND_ASSIGN(Window);
	};
}

#endif
