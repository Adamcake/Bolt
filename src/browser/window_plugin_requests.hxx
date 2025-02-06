#ifndef _BOLT_WINDOW_PLUGIN_REQUESTS_HXX
#define _BOLT_WINDOW_PLUGIN_REQUESTS_HXX
#if defined(BOLT_PLUGINS)
#include "../library/ipc.h"
#include "../file_manager.hxx"
#include "include/cef_request_handler.h"

#include <mutex>

namespace Browser {
	/// Abstract class handling requests from plugin-managed browsers
	struct PluginRequestHandler: public CefRequestHandler {
		PluginRequestHandler(BoltIPCMessageTypeToClient message_type, std::mutex* send_lock):
			message_type(message_type), send_lock(send_lock), current_capture_id(-1) {}

		void HandlePluginMessage(const uint8_t*, size_t);
		void HandleCaptureNotify(uint64_t, uint64_t, int, int, bool);
		void NotifyBrowserCreated(CefRefPtr<CefBrowser>);

		CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
			CefRefPtr<CefBrowser>,
			CefRefPtr<CefFrame>,
			CefRefPtr<CefRequest>,
			bool,
			bool,
			const CefString&,
			bool&
		) override;

		void AddPendingMessage(CefRefPtr<CefProcessMessage>);
		uint64_t CurrentCaptureID() const;
		void SetCurrentCaptureID(uint64_t);

		virtual uint64_t WindowID() const = 0;
		virtual uint64_t PluginID() const = 0;
		virtual BoltSocketType ClientFD() const = 0;
		virtual CefRefPtr<FileManager::FileManager> FileManager() const = 0;
		virtual CefRefPtr<CefBrowser> Browser() const = 0;
		virtual void HandlePluginCloseRequest() = 0;
		virtual void SendCaptureDone() const = 0;
		virtual void HandleShowDevtools() = 0;

		protected:
			std::mutex* send_lock;
			uint64_t current_capture_id;

		private:
			BoltIPCMessageTypeToClient message_type;
			std::vector<CefRefPtr<CefProcessMessage>> pending_messages;
	};
}

#endif
#endif
