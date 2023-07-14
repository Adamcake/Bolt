#ifndef _BOLT_CLIENT_HXX_
#define _BOLT_CLIENT_HXX_
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/views/cef_window_delegate.h"
#include "app.hxx"
#include "../browser.hxx"

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct _InternalFile {
	bool success;
	std::vector<unsigned char> data;
	CefString mime_type;
};

namespace Browser {
	/// Implementation of CefClient, CefBrowserProcessHandler, CefLifeSpanHandler, CefRequestHandler.
	/// Store on the stack, but access only via CefRefPtr.
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_client.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_browser_process_handler.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_life_span_handler.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_request_handler.h
	struct Client: public CefClient, CefBrowserProcessHandler, CefLifeSpanHandler, CefRequestHandler {
		Client(CefRefPtr<Browser::App>, std::filesystem::path);

		/* CefClient overrides */
		CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
		CefRefPtr<CefRequestHandler> GetRequestHandler() override;
		bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefProcessId, CefRefPtr<CefProcessMessage>) override;

		/* CefBrowserProcessHandler overrides */
		void OnContextInitialized() override;

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

			bool show_devtools;
			std::filesystem::path config_dir;
			size_t env_count;

			std::string env_key_home = "HOME=";
			std::string env_key_access_token = "JX_ACCESS_TOKEN=";
			std::string env_key_refresh_token = "JX_REFRESH_TOKEN=";
			std::string env_key_session_id = "JX_SESSION_ID=";
			std::string env_key_character_id = "JX_CHARACTER_ID=";
			std::string env_key_display_name = "JX_DISPLAY_NAME=";
			std::string internal_url = "https://bolt-internal/";
			std::map<std::string, _InternalFile> internal_pages;
#if defined(__linux__)
			std::string launcher_uri = "index.html?platform=linux";
#elif defined(_WIN32)
			std::string launcher_uri = "index.html?platform=windows";
#elif defined(__APPLE__)
			std::string launcher_uri = "index.html?platform=mac";
#endif

			// Mutex-locked vector - may be accessed from either UI thread (most of the time) or IO thread (GetResourceRequestHandler)
			std::vector<CefRefPtr<Browser::Window>> windows;
			std::mutex windows_lock;
	};
}

#endif
