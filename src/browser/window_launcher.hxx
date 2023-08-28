#ifndef _BOLT_WINDOW_LAUNCHER_HXX_
#define _BOLT_WINDOW_LAUNCHER_HXX_

#include "../browser.hxx"
#include "include/cef_resource_handler.h"

#include <filesystem>

namespace Browser {
	struct Launcher: public Window {
		Launcher(CefRefPtr<CefClient>, Details, bool, const std::map<std::string, InternalFile>* const, std::filesystem::path, std::filesystem::path);

		bool IsLauncher() const override;

		CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
			CefRefPtr<CefBrowser>,
			CefRefPtr<CefFrame>,
			CefRefPtr<CefRequest>,
			bool,
			bool,
			const CefString&,
			bool&
		) override;

		/* 
		Functions called by GetResourceRequestHandler. The result will be returned immediately and must not be null.
		The request and URL query string are provided for parsing.
		*/

		CefRefPtr<CefResourceRequestHandler> LaunchRs3Deb(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchRuneliteJar(CefRefPtr<CefRequest>, std::string_view);

		private:
			const std::string internal_url = "https://bolt-internal/";
			const std::map<std::string, InternalFile>* internal_pages;
			std::filesystem::path data_dir;
			std::filesystem::path creds_path;
			std::filesystem::path config_path;
			std::filesystem::path rs3_path;
			std::filesystem::path rs3_hash_path;
			std::filesystem::path runelite_path;
			std::filesystem::path runelite_hash_path;
	};
}

#endif
