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
	const size_t bytes = sizeof(BoltIPCMessageToClient) + sizeof(uint64_t) + sizeof(void*) + (2 *sizeof(int)) + (4 * rect_count * sizeof(int));
	uint8_t* buf = new uint8_t[bytes];
	((BoltIPCMessageToClient*)buf)->message_type = IPC_MSG_OSRUPDATE;
	((BoltIPCMessageToClient*)buf)->items = rect_count;
	*(uint64_t*)(buf + sizeof(BoltIPCMessageToClient)) = id;
	*(void**)(buf + sizeof(BoltIPCMessageToClient) + sizeof(uint64_t)) = needs_remap;
	*(int*)(buf + sizeof(BoltIPCMessageToClient) + sizeof(uint64_t) + sizeof(void*)) = width;
	*(int*)(buf + sizeof(BoltIPCMessageToClient) + sizeof(uint64_t) + sizeof(void*) + sizeof(int)) = height;
	int* rect_data = (int*)(buf + sizeof(BoltIPCMessageToClient) + sizeof(uint64_t) + sizeof(void*) + (2 * sizeof(int)));
	for (uint32_t i = 0; i < rect_count; i += 1) {
		*rect_data = rects[i].x;
		*(rect_data + 1) = rects[i].y;
		*(rect_data + 2) = rects[i].width;
		*(rect_data + 3) = rects[i].height;
		rect_data += 4;
	}
	_bolt_ipc_send(fd, buf, bytes);
	delete[] buf;
}

Browser::WindowOSR::WindowOSR(CefString url, int width, int height, BoltSocketType client_fd, Browser::Client* main_client, int pid, uint64_t window_id, CefRefPtr<FileManager::Directory> file_manager):
	deleted(false), pending_delete(false), client_fd(client_fd), width(width), height(height), browser(nullptr), window_id(window_id),
	main_client(main_client), stored(nullptr), remote_has_remapped(false), remote_is_idle(true), file_manager(file_manager)
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

uint64_t Browser::WindowOSR::ID() {
	return this->window_id;
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

void Browser::WindowOSR::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
	this->browser = browser;
	if (this->pending_delete) {
		browser->GetHost()->CloseBrowser(true);
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

CefRefPtr<CefResourceRequestHandler> Browser::WindowOSR::GetResourceRequestHandler(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	bool is_navigation,
	bool is_download,
	const CefString& request_initiator,
	bool& disable_default_handling
) {
	// Parse URL
	const std::string request_url = request->GetURL().ToString();
	const std::string::size_type colon = request_url.find_first_of(':');
	if (colon == std::string::npos) return nullptr;
	const std::string_view schema(request_url.begin(), request_url.begin() + colon);
	if (schema != "file") return nullptr;
	if (request_url.begin() + colon + 1 == request_url.end()) return nullptr;
	if (request_url.at(colon + 1) != '/') return nullptr;
	const std::string::size_type question_mark = request_url.find_first_of('?');
	std::string_view req_path(request_url.begin() + colon + 1, (question_mark == std::string::npos) ? request_url.end() : request_url.begin() + question_mark);

	FileManager::File file = this->file_manager->get(req_path);
	if (file.contents) {
		return new ResourceHandler(file, this->file_manager);
	} else {
		this->file_manager->free(file);
		const char* data = "Not Found\n";
		return new ResourceHandler(reinterpret_cast<const unsigned char*>(data), strlen(data), 404, "text/plain");
	}
}

#endif
