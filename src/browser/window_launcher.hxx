#ifndef _BOLT_WINDOW_LAUNCHER_HXX_
#define _BOLT_WINDOW_LAUNCHER_HXX_

#include "../browser.hxx"

#include <filesystem>

namespace Browser {
	struct Launcher: public Window {
		Launcher(CefRefPtr<CefClient>, Details, bool, const std::map<std::string, InternalFile>* const, std::filesystem::path);

		CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
			CefRefPtr<CefBrowser>,
			CefRefPtr<CefFrame>,
			CefRefPtr<CefRequest>,
			bool,
			bool,
			const CefString&,
			bool&
		) override;

		private:
			std::string env_key_home = "HOME=";
			std::string env_key_access_token = "JX_ACCESS_TOKEN=";
			std::string env_key_refresh_token = "JX_REFRESH_TOKEN=";
			std::string env_key_session_id = "JX_SESSION_ID=";
			std::string env_key_character_id = "JX_CHARACTER_ID=";
			std::string env_key_display_name = "JX_DISPLAY_NAME=";
			std::string internal_url = "https://bolt-internal/";
			const std::map<std::string, InternalFile>* internal_pages;
			std::filesystem::path data_dir;
			std::filesystem::path hash_path;
			std::filesystem::path rs3_path;
			std::filesystem::path rs3_hash_path;
			size_t env_count;
	};
}

#endif
