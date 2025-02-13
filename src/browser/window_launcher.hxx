#ifndef _BOLT_WINDOW_LAUNCHER_HXX_
#define _BOLT_WINDOW_LAUNCHER_HXX_

#include "../browser.hxx"
#include "../file_manager.hxx"

#include <filesystem>

namespace Browser {
	struct Launcher: public Window, CefRequestHandler {
		Launcher(CefRefPtr<Browser::Client>, Details, bool, CefRefPtr<FileManager::FileManager>, std::filesystem::path, std::filesystem::path);

		CefRefPtr<CefRequestHandler> GetRequestHandler() override;
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

		/// Builds and returns the URL for the launcher to open, including reading config files and
		/// inserting their contents into the query params
		CefString BuildURL() const;

#if defined(BOLT_PLUGINS)
		/// Gets a new game client list and sends it to the browser window via a postMessage.
		/// bool indicates whether client's game_clients_lock is already locked
		void UpdateClientList(bool need_lock_client_mutex) const;
#endif

		void NotifyClosed() override;

		/* 
		Functions called by GetResourceRequestHandler. The result will be returned immediately and must not be null.
		The request and URL query string are provided for parsing.
		*/

		CefRefPtr<CefResourceRequestHandler> LaunchRs3Deb(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchRs3Exe(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchRs3App(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchOsrsExe(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchOsrsApp(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchRuneliteJarNormal(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchRuneliteJarConfigure(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> LaunchRuneliteJar(CefRefPtr<CefRequest>, std::string_view, bool configure);
		CefRefPtr<CefResourceRequestHandler> LaunchHdosJar(CefRefPtr<CefRequest>, std::string_view);

		CefRefPtr<CefResourceRequestHandler> SaveConfig(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> SaveCredentials(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> OpenExternalUrl(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> BrowseData(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> JarFilePicker(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> JsonFilePicker(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> Close(CefRefPtr<CefRequest>, std::string_view);

#if defined(BOLT_PLUGINS)
		CefRefPtr<CefResourceRequestHandler> SavePluginConfig(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> ListGameClients(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> ReadJsonFile(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> StartPlugin(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> StopPlugin(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> InstallPlugin(CefRefPtr<CefRequest>, std::string_view);
		CefRefPtr<CefResourceRequestHandler> GetPluginDirJson(CefRefPtr<CefRequest>, std::string_view);
#endif

		private:
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

CefRefPtr<CefResourceRequestHandler> SaveFileFromPost(CefRefPtr<CefRequest>, const std::filesystem::path::value_type*);
bool BrowseFile(std::filesystem::path&);

#endif
