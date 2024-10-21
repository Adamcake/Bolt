#ifndef _BOLT_WINDOW_OSR_HXX_
#define _BOLT_WINDOW_OSR_HXX_
#if defined(BOLT_PLUGINS)

#include "../library/ipc.h"

#include "include/cef_base.h"
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_render_handler.h"

#include "window_plugin_requests.hxx"
#include "../file_manager/directory.hxx"

#include <mutex>

struct RepositionEvent;
struct MouseMotionEvent;
struct MouseButtonEvent;
struct MouseScrollEvent;

namespace Browser {
	struct Client;

	struct WindowOSR: public CefClient, CefLifeSpanHandler, CefRenderHandler, PluginRequestHandler {
		WindowOSR(CefString url, int width, int height, BoltSocketType client_fd, Client* main_client, std::mutex* send_lock, int pid, uint64_t window_id, uint64_t plugin_id, CefRefPtr<FileManager::Directory>);

		bool IsDeleted();

		void Close();

		void HandleAck();
		void HandleReposition(const RepositionEvent*);
		void HandleMouseMotion(const MouseMotionEvent*);
		void HandleMouseButton(const MouseButtonEvent*);
		void HandleMouseButtonUp(const MouseButtonEvent*);
		void HandleScroll(const MouseScrollEvent*);
		void HandleMouseLeave(const MouseMotionEvent*);

		uint64_t WindowID() const override;
		uint64_t PluginID() const override;
		BoltSocketType ClientFD() const override;
		CefRefPtr<FileManager::FileManager> FileManager() const override;
		CefRefPtr<CefBrowser> Browser() const override;
		void HandlePluginCloseRequest() override;

		CefRefPtr<CefRequestHandler> GetRequestHandler() override;
		CefRefPtr<CefRenderHandler> GetRenderHandler() override;
		CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
		bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefProcessId, CefRefPtr<CefProcessMessage>) override;

		void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
		void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override;
		void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) override;
		void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height) override;

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
		void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
		void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

		private:
			bool deleted;
			bool pending_delete;
			BoltSocketType client_fd;
			std::mutex size_lock;
			int width, height;
#if defined(_WIN32)
			HANDLE shm;
			HANDLE target_process;
#else
			int shm;
#endif
			void* file;
			size_t mapping_size;
			CefRefPtr<CefBrowser> browser;
			uint64_t window_id;
			uint64_t plugin_id;
			Client* main_client;
			CefRefPtr<FileManager::Directory> file_manager;

			std::mutex stored_lock;
			void* stored;
			int stored_width;
			int stored_height;
			CefRect stored_damage;
			uint8_t remote_has_remapped;
			uint8_t remote_is_idle;

			IMPLEMENT_REFCOUNTING(WindowOSR);
			DISALLOW_COPY_AND_ASSIGN(WindowOSR);
	};
}

#endif
#endif
