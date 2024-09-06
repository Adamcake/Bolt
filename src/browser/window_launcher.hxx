#ifndef _BOLT_WINDOW_LAUNCHER_HXX_
#define _BOLT_WINDOW_LAUNCHER_HXX_

#include "../browser.hxx"
#include "../file_manager.hxx"

#include "include/cef_parser.h"

#include <filesystem>
#include <functional>

namespace Browser {
	struct Launcher: public Window {
		Launcher(CefRefPtr<Browser::Client>, Details, bool, CefRefPtr<FileManager::FileManager>, std::filesystem::path, std::filesystem::path);

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

		void OnBrowserDestroyed(CefRefPtr<CefBrowserView>, CefRefPtr<CefBrowser>) override;

		void Refresh() const override;

		/// Attempts to open the given URL externally in the user's browser
		void OpenExternalUrl(char* url) const;

		/// Attempts to open Bolt's data directory externally in the user's file explorer.
		/// Returns true on success, false on failure.
		bool BrowseData() const;

		/// Builds and returns the URL for the launcher to open, including reading config files and
		/// inserting their contents into the query params
		CefString BuildURL() const;

		/// Goes through all the key-value pairs in the given query string and calls the callback for each one.
		void ParseQuery(std::string_view query, std::function<void(const std::string_view&, const std::string_view&)> callback);

		/* 
		Functions called by GetResourceRequestHandler. The result will be returned immediately and must not be null.
		The request and URL query string are provided for parsing.
		*/

		CefRefPtr<CefResourceRequestHandler> LaunchRs3Deb(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchRs3Exe(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchRs3App(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchOsrsExe(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchOsrsApp(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchRuneliteJar(CefRefPtr<CefRequest>, std::string_view, bool configure);
		CefRefPtr<CefResourceRequestHandler> LaunchHdosJar(CefRefPtr<CefRequest>, std::string_view);

		private:
			const std::string internal_url = "https://bolt-internal/";
			CefRefPtr<FileManager::FileManager> file_manager;
			std::filesystem::path data_dir;
			std::filesystem::path creds_path;
			std::filesystem::path config_path;
			std::filesystem::path rs3_elf_path;
			std::filesystem::path rs3_elf_hash_path;
			std::filesystem::path rs3_exe_path;
			std::filesystem::path rs3_exe_hash_path;
			std::filesystem::path rs3_app_path;
			std::filesystem::path rs3_app_hash_path;
			std::filesystem::path osrs_exe_path;
			std::filesystem::path osrs_exe_hash_path;
			std::filesystem::path osrs_app_path;
			std::filesystem::path osrs_app_hash_path;
			std::filesystem::path runelite_path;
			std::filesystem::path runelite_id_path;
			std::filesystem::path hdos_path;
			std::filesystem::path hdos_version_path;
#if defined(BOLT_PLUGINS)
			std::filesystem::path plugin_config_path;
#endif
	};
}

#if defined(_WIN32)
typedef std::wstring QSTRING;
#define PQTOSTRING ToWString
#else
typedef std::string QSTRING;
#define PQTOSTRING ToString
#endif

#define PQCEFSTRING(KEY) \
if (key == #KEY) { \
	has_##KEY = true; \
	KEY = CefURIDecode(std::string(val), true, (cef_uri_unescape_rule_t)(UU_SPACES | UU_PATH_SEPARATORS | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS | UU_REPLACE_PLUS_WITH_SPACE)); \
	return; \
}

#define PQSTRING(KEY) \
if (key == #KEY) { \
	has_##KEY = true; \
	KEY = CefURIDecode(std::string(val), true, (cef_uri_unescape_rule_t)(UU_SPACES | UU_PATH_SEPARATORS | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS | UU_REPLACE_PLUS_WITH_SPACE)).PQTOSTRING(); \
	return; \
}

#define PQINT(KEY) \
if (key == #KEY) { \
	has_##KEY = true; \
	KEY##_valid = true; \
	KEY = 0; \
	for (auto it = val.begin(); it != val.end(); it += 1) { \
		if (*it < '0' || *it > '9') { \
			KEY##_valid = false; \
			return; \
		} \
		KEY = (KEY * 10) + (*it - '0'); \
	} \
	return; \
}

#define PQBOOL(KEY) \
if (key == #KEY) { \
	KEY = (val.size() > 0 && val != "0"); \
	return; \
}

#define QSENDSTR(STR, CODE) return new Browser::ResourceHandler(reinterpret_cast<const unsigned char*>(STR "\n"), sizeof(STR "\n") - sizeof(*STR), CODE, "text/plain")
#define QSENDMOVED(LOC) return new Browser::ResourceHandler(reinterpret_cast<const unsigned char*>("Moved\n"), sizeof("Moved\n") - sizeof("Moved\n"), 302, "text/plain", LOC)
#define QSENDOK() QSENDSTR("OK", 200)
#define QSENDNOTFOUND() QSENDSTR("Not found", 404)
#define QSENDBADREQUESTIF(COND) if (COND) QSENDSTR("Bad response", 400)
#define QSENDSYSTEMERRORIF(COND) if (COND) QSENDSTR("System error", 500)
#define QSENDNOTSUPPORTED() QSENDSTR("Not supported", 400)
#define QREQPARAM(NAME) if (!has_##NAME) QSENDSTR("Missing required param " #NAME, 400)
#define QREQPARAMINT(NAME) QREQPARAM(NAME); if (!NAME##_valid) QSENDSTR("Invalid value for required param " #NAME, 400)

#endif
