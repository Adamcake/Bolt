#ifndef _BOLT_WINDOW_OSR_HXX_
#define _BOLT_WINDOW_OSR_HXX_
#if defined(BOLT_PLUGINS)

#include "include/cef_base.h"
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_render_handler.h"

#include "../library/ipc.h"

#include <mutex>

namespace Browser {
	struct Client;

	struct WindowOSR: public CefClient, CefLifeSpanHandler, CefRenderHandler {
		WindowOSR(CefString url, int width, int height, BoltSocketType client_fd, Client* main_client, int pid, uint64_t window_id);

		bool IsDeleted();

		void Close();

		void HandleAck();

		uint64_t ID();

		CefRefPtr<CefRenderHandler> GetRenderHandler() override;
		void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
		void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height) override;

		void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
		void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

		private:
			bool deleted;
			bool pending_delete;
			BoltSocketType client_fd;
			int width, height;
			int shm;
			void* file;
			size_t mapping_size;
			CefRefPtr<CefBrowser> browser;
			uint64_t window_id;
			Client* main_client;

			std::mutex stored_lock;
			void* stored;
			int stored_width;
			int stored_height;
			uint8_t remote_is_idle;

			IMPLEMENT_REFCOUNTING(WindowOSR);
			DISALLOW_COPY_AND_ASSIGN(WindowOSR);
	};
}

#endif
#endif
