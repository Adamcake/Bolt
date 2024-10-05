#include "window_osr.hxx"

#if defined(BOLT_PLUGINS)
#if defined(_WIN32)
#include <Windows.h>
#else
#define _GNU_SOURCE 1
#include <unistd.h>
#include <sys/mman.h>
#include <cerrno>
#include <fcntl.h>
#endif

#include <fmt/core.h>
#include "client.hxx"
#include "resource_handler.hxx"
#include "../library/event.h"

static void SendUpdateMsg(BoltSocketType fd, uint64_t id, int width, int height, void* needs_remap, const CefRect* rects, uint32_t rect_count) {
	const size_t bytes = sizeof(BoltIPCMessageTypeToClient) + sizeof(BoltIPCOsrUpdateHeader) + (rect_count * sizeof(BoltIPCOsrUpdateRect));
	uint8_t* buf = new uint8_t[bytes];
	BoltIPCMessageTypeToClient* msg_type = reinterpret_cast<BoltIPCMessageTypeToClient*>(buf);
	BoltIPCOsrUpdateHeader* header = reinterpret_cast<BoltIPCOsrUpdateHeader*>(msg_type + 1);
	BoltIPCOsrUpdateRect* ipc_rects = reinterpret_cast<BoltIPCOsrUpdateRect*>(header + 1);
	*msg_type = IPC_MSG_OSRUPDATE;
	*header = { .rect_count = rect_count, .window_id = id, .needs_remap = needs_remap, .width = width, .height = height };

	for (uint32_t i = 0; i < rect_count; i += 1) {
		ipc_rects[i] = { .x = rects[i].x, .y = rects[i].y, .w = rects[i].width, .h = rects[i].height };
	}
	_bolt_ipc_send(fd, buf, bytes);
	delete[] buf;
}

Browser::WindowOSR::WindowOSR(CefString url, int width, int height, BoltSocketType client_fd, Browser::Client* main_client, int pid, uint64_t window_id, uint64_t plugin_id, CefRefPtr<FileManager::Directory> file_manager):
	PluginRequestHandler(IPC_MSG_OSRBROWSERMESSAGE),
	deleted(false), pending_delete(false), client_fd(client_fd), width(width), height(height), browser(nullptr), window_id(window_id),
	plugin_id(plugin_id), main_client(main_client), stored(nullptr), remote_has_remapped(false), remote_is_idle(true), file_manager(file_manager)
{
	this->mapping_size = (size_t)width * (size_t)height * 4;

#if defined(_WIN32)
	this->shm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)this->mapping_size, NULL);
	this->target_process = OpenProcess(PROCESS_DUP_HANDLE, 0, pid);
	this->file = MapViewOfFile(this->shm, FILE_MAP_WRITE, 0, 0, this->mapping_size);
#else
	char buf[256];
	snprintf(buf, sizeof(buf), "/bolt-%i-eb-%lu", pid, window_id);
	this->shm = shm_open(buf, O_RDWR, 644);
	shm_unlink(buf);
	if (ftruncate(this->shm, this->mapping_size)) {
		fmt::print("[B] WindowOSR(): ftruncate error {}\n", errno);
	}
	this->file = mmap(nullptr, this->mapping_size, PROT_WRITE, MAP_SHARED, this->shm, 0);
	if (this->file == MAP_FAILED) {
		fmt::print("[B] WindowOSR(): mmap error {}\n", errno);
	}
#endif

	CefWindowInfo window_info;
	window_info.bounds.width = width;
	window_info.bounds.height = height;
	window_info.SetAsWindowless(0);
	CefBrowserSettings browser_settings;
	browser_settings.background_color = CefColorSetARGB(0, 0, 0, 0);
	CefBrowserHost::CreateBrowser(window_info, this, CefString(url), browser_settings, nullptr, nullptr);
}

bool Browser::WindowOSR::IsDeleted() {
	return this->deleted;
}

void Browser::WindowOSR::Close() {
	if (this->browser) {
		this->browser->GetHost()->CloseBrowser(true);
	} else {
		this->pending_delete = true;
	}
}

uint64_t Browser::WindowOSR::WindowID() const {
	return this->window_id;
}

uint64_t Browser::WindowOSR::PluginID() const {
	return this->plugin_id;
}

BoltSocketType Browser::WindowOSR::ClientFD() const {
	return this->client_fd;
}

CefRefPtr<FileManager::FileManager> Browser::WindowOSR::FileManager() const {
	return this->file_manager;
}

CefRefPtr<CefBrowser> Browser::WindowOSR::Browser() const {
	return this->browser;
}

void Browser::WindowOSR::HandleAck() {
	if (this->deleted) return;
	this->stored_lock.lock();
	if (this->stored) {
		const size_t length = this->stored_width * this->stored_height * 4;
		void* needs_remap = length != this->mapping_size ? (void*)1 : (void*)0;
		if (needs_remap) {
#if defined(_WIN32)
			UnmapViewOfFile(this->file);
			CloseHandle(this->shm);
			this->shm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)length, NULL);
			this->file = MapViewOfFile(this->shm, FILE_MAP_WRITE, 0, 0, length);
			DuplicateHandle(GetCurrentProcess(), this->shm, this->target_process, (LPHANDLE)&needs_remap, 0, false, DUPLICATE_SAME_ACCESS);
#else
			if (ftruncate(this->shm, length)) {
				fmt::print("[B] OnPaint: ftruncate error {}\n", errno);
			}
			this->file = mremap(this->file, this->mapping_size, length, MREMAP_MAYMOVE);
#endif
			this->mapping_size = length;
		}
		memcpy(this->file, this->stored, length);
		SendUpdateMsg(this->client_fd, this->window_id, this->stored_width, this->stored_height, needs_remap, &this->stored_damage, 1);
		::free(this->stored);
		this->stored = nullptr;
	} else {
		this->remote_is_idle = true;
	}
	this->stored_lock.unlock();
}

void Browser::WindowOSR::HandleResize(const ResizeEvent* event) {
	this->size_lock.lock();
	this->width = event->width;
	this->height = event->height;
	this->size_lock.unlock();
	if (this->browser) this->browser->GetHost()->WasResized();
}

static void MouseEventToCef(const MouseEvent* in, CefMouseEvent* out) {
	out->x = in->x;
	out->y = in->y;
	out->modifiers =
		in->capslock ? EVENTFLAG_CAPS_LOCK_ON : 0 |
		in->shift ? EVENTFLAG_SHIFT_DOWN : 0 |
		in->ctrl ? EVENTFLAG_CONTROL_DOWN : 0 |
		in->alt ? EVENTFLAG_ALT_DOWN : 0 |
		in->mb_left ? EVENTFLAG_LEFT_MOUSE_BUTTON : 0 |
		in->mb_middle ? EVENTFLAG_MIDDLE_MOUSE_BUTTON : 0 |
		in->mb_right ? EVENTFLAG_RIGHT_MOUSE_BUTTON : 0 |
		in->meta ? EVENTFLAG_COMMAND_DOWN : 0 |
		in->numlock ? EVENTFLAG_NUM_LOCK_ON : 0;
}

static cef_mouse_button_type_t MouseButtonToCef(uint8_t in) {
	switch (in) {
		default:
		case 1:
			return MBT_LEFT;
		case 2:
			return MBT_RIGHT;
		case 3:
			return MBT_MIDDLE;
	}
}

void Browser::WindowOSR::HandleMouseMotion(const MouseMotionEvent* event) {
	CefMouseEvent e;
	MouseEventToCef(&event->details, &e);
	if (this->browser) this->browser->GetHost()->SendMouseMoveEvent(e, false);
}

void Browser::WindowOSR::HandleMouseButton(const MouseButtonEvent* event) {
	CefMouseEvent e;
	MouseEventToCef(&event->details, &e);
	if (this->browser) this->browser->GetHost()->SendMouseClickEvent(e, MouseButtonToCef(event->button), false, 1);
}

void Browser::WindowOSR::HandleMouseButtonUp(const MouseButtonEvent* event) {
	CefMouseEvent e;
	MouseEventToCef(&event->details, &e);
	if (this->browser) this->browser->GetHost()->SendMouseClickEvent(e, MouseButtonToCef(event->button), true, 1);
}

void Browser::WindowOSR::HandleScroll(const MouseScrollEvent* event) {
	CefMouseEvent e;
	MouseEventToCef(&event->details, &e);
	if (this->browser) this->browser->GetHost()->SendMouseWheelEvent(e, 0, event->direction ? 20 : -20);
}

void Browser::WindowOSR::HandleMouseLeave(const MouseMotionEvent* event) {
	CefMouseEvent e;
	MouseEventToCef(&event->details, &e);
	if (this->browser) this->browser->GetHost()->SendMouseMoveEvent(e, true);
}

CefRefPtr<CefRequestHandler> Browser::WindowOSR::GetRequestHandler() {
	return this;
}

CefRefPtr<CefRenderHandler> Browser::WindowOSR::GetRenderHandler() {
	return this;
}

CefRefPtr<CefLifeSpanHandler> Browser::WindowOSR::GetLifeSpanHandler() {
	return this;
}

bool Browser::WindowOSR::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId process, CefRefPtr<CefProcessMessage> message) {
	if (!(this->browser && this->browser->IsSame(browser))) return false;
	const CefString name = message->GetName();
	if (name == "__bolt_pluginbrowser_close") {
		this->browser->GetHost()->CloseBrowser(true);
	}
	return false;
}

void Browser::WindowOSR::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
	this->size_lock.lock();
	rect.Set(0, 0, this->width, this->height);
	this->size_lock.unlock();
}

void Browser::WindowOSR::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height) {
	if (this->deleted || dirtyRects.empty()) return;

	const size_t length = (size_t)width * (size_t)height * 4;
	this->stored_lock.lock();
	if (this->remote_is_idle) {
		void* needs_remap = length != this->mapping_size ? (void*)1 : (void*)0;
		if (needs_remap) {
#if defined(_WIN32)
			UnmapViewOfFile(this->file);
			CloseHandle(this->shm);
			this->shm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)length, NULL);
			this->file = MapViewOfFile(this->shm, FILE_MAP_WRITE, 0, 0, length);
			DuplicateHandle(GetCurrentProcess(), this->shm, this->target_process, (LPHANDLE)&needs_remap, 0, false, DUPLICATE_SAME_ACCESS);
			this->remote_has_remapped = 1;
#else
			if (ftruncate(this->shm, length)) {
				fmt::print("[B] OnPaint: ftruncate error {}\n", errno);
			}
			this->file = mremap(this->file, this->mapping_size, length, MREMAP_MAYMOVE);
#endif
			this->mapping_size = length;
			memcpy(this->file, buffer, length);
		} else {
			for (const CefRect& rect: dirtyRects) {
				for (int y = rect.y; y < rect.y + rect.height; y += 1) {
					const int offset = (y * width * 4) + (rect.x * 4);
					memcpy((uint8_t*)this->file + offset, (uint8_t*)buffer + offset, rect.width * 4);
				}
			}
		}
		if (!this->remote_has_remapped) {
#if defined(_WIN32)
			DuplicateHandle(GetCurrentProcess(), this->shm, this->target_process, (LPHANDLE)&needs_remap, 0, false, DUPLICATE_SAME_ACCESS);
#else
			needs_remap = (void*)1;
#endif
		}
		SendUpdateMsg(this->client_fd, this->window_id, width, height, needs_remap, dirtyRects.data(), dirtyRects.size());
		this->remote_has_remapped = true;
		this->remote_is_idle = false;
	} else {
		const bool is_damage_stored = this->stored != nullptr;
		if (is_damage_stored) ::free(this->stored);
		this->stored = malloc(length);
		memcpy(this->stored, buffer, length);
		this->stored_width = width;
		this->stored_height = height;

		if (!is_damage_stored) this->stored_damage = dirtyRects[0];
		for (size_t i = is_damage_stored ? 0 : 1; i < dirtyRects.size(); i += 1) {
			if (dirtyRects[i].x < this->stored_damage.x) {
				this->stored_damage.width += this->stored_damage.x - dirtyRects[i].x;
				this->stored_damage.x = dirtyRects[i].x;
			}
			if (dirtyRects[i].y < this->stored_damage.y) {
				this->stored_damage.height += this->stored_damage.y - dirtyRects[i].y;
				this->stored_damage.y = dirtyRects[i].y;
			}
			if ((dirtyRects[i].x + dirtyRects[i].width) > (this->stored_damage.x + this->stored_damage.width)) {
				this->stored_damage.width += (dirtyRects[i].x + dirtyRects[i].width) - (this->stored_damage.x + this->stored_damage.width);
			}
			if ((dirtyRects[i].y + dirtyRects[i].height) > (this->stored_damage.y + this->stored_damage.height)) {
				this->stored_damage.height += (dirtyRects[i].y + dirtyRects[i].height) - (this->stored_damage.y + this->stored_damage.height);
			}
		}
	}
	this->stored_lock.unlock();
}

bool Browser::WindowOSR::OnBeforePopup(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	const CefString& target_url,
	const CefString& target_frame_name,
	CefLifeSpanHandler::WindowOpenDisposition target_disposition,
	bool user_gesture,
	const CefPopupFeatures& popup_features,
	CefWindowInfo& window_info,
	CefRefPtr<CefClient>& client,
	CefBrowserSettings& settings,
	CefRefPtr<CefDictionaryValue>& extra_info,
	bool* no_javascript_access
) {
	// block popup creation: osr windows can't have child windows
	return true;
}

void Browser::WindowOSR::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
	this->browser = browser;
	if (this->pending_delete) {
		// calling CloseBrowser here would lead to a segmentation fault in CEF because we're still
		// technically in the create function, which is going to assume the browser still exists.
		browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, CefProcessMessage::Create("__bolt_pluginbrowser_close"));
	}
}

void Browser::WindowOSR::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	if (browser->IsSame(this->browser)) {
		this->browser = nullptr;
		this->file_manager = nullptr;
		::free(this->stored);
#if defined(_WIN32)
		UnmapViewOfFile(this->file);
		CloseHandle(this->shm);
		CloseHandle(this->target_process);
#else
		munmap(this->file, this->mapping_size);
		close(this->shm);
#endif
		this->deleted = true;
		main_client->CleanupClientPlugins(this->client_fd);
	}
}

#endif
