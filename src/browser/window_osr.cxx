#include "window_osr.hxx"

#if defined(BOLT_PLUGINS)
#if defined(_WIN32)
// ?
#else
#define _GNU_SOURCE 1
#include <unistd.h>
#include <sys/mman.h>
#include <cerrno>
#include <fcntl.h>
#endif

#include <fmt/core.h>
#include "client.hxx"

static void SendUpdateMsg(BoltSocketType fd, uint64_t id, int width, int height, bool needs_remap, const CefRect* rects, uint32_t rect_count) {
	uint8_t buf[sizeof(BoltIPCMessageToClient) + sizeof(uint64_t) + (2 *sizeof(int)) + (rect_count * sizeof(int) * 4) + 1];
	((BoltIPCMessageToClient*)buf)->message_type = IPC_MSG_OSRUPDATE;
	((BoltIPCMessageToClient*)buf)->items = rect_count;
	*(uint64_t*)(buf + sizeof(BoltIPCMessageToClient)) = id;
	*(uint8_t*)(buf + sizeof(BoltIPCMessageToClient) + sizeof(uint64_t)) = needs_remap ? 1 : 0;
	*(int*)(buf + sizeof(BoltIPCMessageToClient) + sizeof(uint64_t) + 1) = width;
	*(int*)(buf + sizeof(BoltIPCMessageToClient) + sizeof(uint64_t) + sizeof(int) + 1) = height;
	int* rect_data = (int*)(buf + sizeof(BoltIPCMessageToClient) + sizeof(uint64_t) + (sizeof(int) * 2) + 1);
	for (uint32_t i = 0; i < rect_count; i += 1) {
		*rect_data = rects[i].x;
		*(rect_data + 1) = rects[i].y;
		*(rect_data + 2) = rects[i].width;
		*(rect_data + 3) = rects[i].height;
		rect_data += 4;
	}
	_bolt_ipc_send(fd, buf, sizeof(buf));
}

Browser::WindowOSR::WindowOSR(CefString url, int width, int height, BoltSocketType client_fd, Browser::Client* main_client, int pid, uint64_t window_id):
	deleted(false), pending_delete(false), client_fd(client_fd), width(width), height(height), browser(nullptr), window_id(window_id),
	main_client(main_client), stored(nullptr), remote_is_idle(true)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "/bolt-%i-eb-%lu", pid, window_id);
	this->shm = shm_open(buf, O_RDWR, 644);
	shm_unlink(buf);
	const off_t shm_len = (off_t)width * (off_t)height * 4;
	if (ftruncate(this->shm, shm_len)) {
		fmt::print("[B] WindowOSR(): ftruncate error {}\n", errno);
	}
	this->file = mmap(nullptr, shm_len, PROT_WRITE, MAP_SHARED, this->shm, 0);
	if (this->file == MAP_FAILED) {
		fmt::print("[B] WindowOSR(): mmap error {}\n", errno);
	}
	this->mapping_size = shm_len;

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

void Browser::WindowOSR::HandleAck() {
	if (this->deleted) return;
	this->stored_lock.lock();
	if (this->stored) {
		const int length = this->stored_width * this->stored_height * 4;
		const bool needs_remap = length != this->mapping_size;
		if (needs_remap) {
			this->file = mremap(nullptr, this->mapping_size, length, MAP_SHARED);
			this->mapping_size = length;
		}
		memcpy(this->file, this->stored, length);
		SendUpdateMsg(this->client_fd, this->window_id, this->stored_width, this->stored_height, needs_remap, &this->stored_damage, 1);
		free(this->stored);
		this->stored = nullptr;
	} else {
		this->remote_is_idle = true;
	}
	this->stored_lock.unlock();
}

uint64_t Browser::WindowOSR::ID() {
	return this->window_id;
}

CefRefPtr<CefRenderHandler> Browser::WindowOSR::GetRenderHandler() {
	return this;
}

void Browser::WindowOSR::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
	rect.Set(0, 0, this->width, this->height);
}

void Browser::WindowOSR::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height) {
	if (this->deleted || dirtyRects.empty()) return;

	const off_t length = width * height * 4;
	this->stored_lock.lock();
	if (this->remote_is_idle) {
		const bool needs_remap = length != this->mapping_size;
		if (needs_remap) {
			if (ftruncate(this->shm, length)) {
				fmt::print("[B] OnPaint: ftruncate error {}\n", errno);
			}
			this->file = mremap(nullptr, this->mapping_size, length, MAP_SHARED);
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
		SendUpdateMsg(this->client_fd, this->window_id, width, height, needs_remap, dirtyRects.data(), dirtyRects.size());
		this->remote_is_idle = false;
	} else {
		const bool is_damage_stored = this->stored != nullptr;
		if (is_damage_stored) free(this->stored);
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
		free(this->stored);
		munmap(this->file, this->mapping_size);
		close(this->shm);
		this->deleted = true;
		main_client->CleanupClientPlugins(this->client_fd);
	}
}

#endif
