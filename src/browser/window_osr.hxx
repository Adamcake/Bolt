#ifndef _BOLT_WINDOW_OSR_HXX_
#define _BOLT_WINDOW_OSR_HXX_
#if defined(BOLT_PLUGINS)

#include "../library/ipc.h"

#include "include/cef_base.h"
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_render_handler.h"

#include "../file_manager/directory.hxx"

#include <mutex>

struct ResizeEvent;
struct MouseMotionEvent;
struct MouseButtonEvent;
struct MouseScrollEvent;

namespace Browser {
	struct Client;

	struct WindowOSR: public CefClient, CefLifeSpanHandler, CefRenderHandler, CefRequestHandler {
		WindowOSR(CefString url, int width, int height, BoltSocketType client_fd, Client* main_client, int pid, uint64_t window_id, CefRefPtr<FileManager::Directory>);

		bool IsDeleted();

		void Close();

		void HandleAck();
		void HandleResize(const ResizeEvent*);
		void HandleMouseMotion(const MouseMotionEvent*);
		void HandleMouseButton(const MouseButtonEvent*);
		void HandleMouseButtonUp(const MouseButtonEvent*);
		void HandleScroll(const MouseScrollEvent*);
		void HandleMouseLeave(const MouseMotionEvent*);
		void HandlePluginMessage(const uint8_t*, size_t);

		uint64_t ID();

		CefRefPtr<CefRequestHandler> GetRequestHandler() override;
		CefRefPtr<CefRenderHandler> GetRenderHandler() override;
		CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
		bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefProcessId, CefRefPtr<CefProcessMessage>) override;

		void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
		void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height) override;

		void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
		void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

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
