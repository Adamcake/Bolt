#ifndef _BOLT_CLIENT_HXX_
#define _BOLT_CLIENT_HXX_
#include "include/base/cef_macros.h"
#if defined(BOLT_PLUGINS)
#include "window_osr.hxx"
#include "../library/ipc.h"
#include <thread>
#endif

#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/views/cef_window_delegate.h"
#include "app.hxx"
#include "../browser.hxx"

#include <filesystem>
#include <mutex>
#include <vector>

#if defined(CEF_X11)
#include <xcb/xcb.h>
#endif

#if defined(BOLT_DEV_LAUNCHER_DIRECTORY)
#include "../file_manager/directory.hxx"
typedef FileManager::Directory CLIENT_FILEHANDLER;
#else
#include "../file_manager/launcher.hxx"
typedef FileManager::Launcher CLIENT_FILEHANDLER;
#endif

namespace Browser {
	/// Implementation of CefClient, CefBrowserProcessHandler, CefLifeSpanHandler, CefRequestHandler.
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_client.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_browser_process_handler.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_life_span_handler.h
	/// https://github.com/chromiumembedded/cef/blob/5735/include/cef_request_handler.h
	/// If building wqith plugin support, also a CefWindowDelegate for the purpose of hosting a dummy IPC frame
	/// https://github.com/chromiumembedded/cef/blob/5735/include/views/cef_window_delegate.h
	struct Client: public CefClient, CefBrowserProcessHandler, CefLifeSpanHandler, CefRequestHandler, CLIENT_FILEHANDLER
#if defined(BOLT_PLUGINS)
	, CefWindowDelegate
#endif
	{
		Client(CefRefPtr<Browser::App>, std::filesystem::path config_dir, std::filesystem::path data_dir, std::filesystem::path runtime_dir);

		/// Either opens a launcher window, or focuses an existing one. No more than one launcher window
		/// may be open at a given time. A launcher window will be opened automatically on startup,
		/// but this function may be used to open another after previous ones have been closed.
		void OpenLauncher();

		/// Checks if it's safe to call CefQuitMessageLoop() and exit the process, and if so, does so.
		/// Implementation differs depending on which build features are enabled.
		void TryExit();

		/// Handler to be called when a new CefWindow is created. Must be called before Show()
		void OnBoltWindowCreated(CefRefPtr<CefWindow>);

#if defined(BOLT_PLUGINS)
		/// Creates and binds an IPC socket, ready for IPCRun() to accept connections - OS-specific
		void IPCBind();

		/// Repeatedly blocks on new client connections and handles them - OS-specific
		void IPCRun();

		/// Stops and joins the IPC thread - OS specific
		void IPCStop();

		/// Handles a new client connecting to the IPC socket. Called by the IPC thread.
		void IPCHandleNewClient(int fd);

		/// Handles any case where the UI for the client list should be reloaded.
		/// Should be called after updating it (e.g. by calling IPCHandleNewClient)
		void IPCHandleClientListUpdate();

		/// Handles a client's file descriptor having been closed
		void IPCHandleClosed(int fd);

		/// Handles the case where a client disconnects and no more clients are connected.
		/// This is separate from IPCHandleClientListUpdate and must be called separately.
		void IPCHandleNoMoreClients();

		/// ResourceRequestHandler style function to return game_clients as JSON
		CefRefPtr<CefResourceRequestHandler> ListGameClients();

		/// Handles a new message being sent to the IPC socket by a client. Called by the IPC thread.
		/// The message hasn't actually been pulled from the socket yet when this function is called;
		/// rather this function will pull it using ipc_receive from ipc.h.
		///
		/// Returns true on success, false on failure.
		bool IPCHandleMessage(int fd);

		/// Sends an IPC message to the named client to start a plugin.
		void StartPlugin(uint64_t client_id, std::string id, std::string path, std::string main);

		/// Filters the list of plugins and their associated browsers for the client identified by the given fd,
		/// to remove deleted plugins and browsers. Usually called when a browser is closed.
		void CleanupClientPlugins(int fd);

		/* CefWindowDelegate overrides */
		void OnWindowCreated(CefRefPtr<CefWindow>) override;
#endif

#if defined(BOLT_DEV_LAUNCHER_DIRECTORY)
		/* FileManager override */
		void OnFileChange() override;
#endif

		/* Icon functions, implementations are generated by the build system from image file contents */
		const unsigned char* GetTrayIcon() const;
		const unsigned char* GetIcon16() const;
		const unsigned char* GetIcon32() const;
		const unsigned char* GetIcon64() const;
		const unsigned char* GetIcon256() const;

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

		private:
			DISALLOW_COPY_AND_ASSIGN(Client);
			IMPLEMENT_REFCOUNTING(Client);

			void Exit();

			bool show_devtools;
			std::filesystem::path config_dir;
			std::filesystem::path data_dir;
			std::filesystem::path runtime_dir;

#if defined(CEF_X11)
			xcb_connection_t* xcb;
#endif

#if defined(BOLT_PLUGINS)
			struct ActivePlugin: ::FileManager::Directory {
				ActivePlugin(uint64_t, std::string, std::filesystem::path);
				std::string id; // unique string generated by fromtend with crypto.randomUUID(), refers to this plugin
				uint64_t uid; // unique incremental integer, refers to this specific activation of this plugin
				bool deleted;
				std::vector<CefRefPtr<Browser::WindowOSR>> windows_osr;

				private:
					IMPLEMENT_REFCOUNTING(ActivePlugin);
					DISALLOW_COPY_AND_ASSIGN(ActivePlugin);
			};
			struct GameClient {
				uint64_t uid;
				int fd;
				bool deleted;
				// identity may be null if game hasn't reported its identity yet or display name is unset
				char* identity;

				std::vector<CefRefPtr<ActivePlugin>> plugins;
			};
			std::thread ipc_thread;
			BoltSocketType ipc_fd;
			CefRefPtr<CefBrowserView> ipc_view;
			CefRefPtr<CefWindow> ipc_window;
			CefRefPtr<CefBrowser> ipc_browser;
			uint64_t next_client_uid;
			uint64_t next_plugin_uid;
			std::mutex game_clients_lock;
			std::vector<GameClient> game_clients;
			CefRefPtr<Browser::WindowOSR> GetWindowFromFDAndID(GameClient* client, uint64_t id);
			CefRefPtr<ActivePlugin> GetPluginFromFDAndID(GameClient* client, uint64_t id);
#endif

			// Mutex-locked vector - may be accessed from either UI thread (most of the time) or IO thread (GetResourceRequestHandler)
			std::vector<CefRefPtr<Browser::Window>> windows;
			std::mutex windows_lock;
	};
}

#endif
