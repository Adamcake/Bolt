#include "plugin.h"

#include <Windows.h>

static void shm_map_readwrite(struct BoltSHM* shm, size_t size) {
    wchar_t buf[256];
    _snwprintf(buf, 256, L"/bolt-%i-%hs-%llu", getpid(), shm->tag, shm->id);
    shm->handle = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)size, buf);
    shm->file = MapViewOfFile(shm->handle, FILE_MAP_WRITE, 0, 0, size);
}

uint8_t _bolt_plugin_shm_open_inbound(struct BoltSHM* shm, const char* tag, uint64_t id) {
    shm->handle = NULL;
    shm->file = NULL;
    shm->tag = tag;
    shm->id = id;
    return 1;
}

uint8_t _bolt_plugin_shm_open_outbound(struct BoltSHM* shm, size_t size, const char* tag, uint64_t id) {
    shm->tag = tag;
    shm->id = id;
    shm_map_readwrite(shm, size);
    return 1;
}

void _bolt_plugin_shm_close(struct BoltSHM* shm) {
    if (shm->file) {
        UnmapViewOfFile(shm->file);
    }
    if (shm->handle) {
        CloseHandle(shm->handle);
    }
}

void _bolt_plugin_shm_resize(struct BoltSHM* shm, size_t size, uint64_t new_id) {
    UnmapViewOfFile(shm->file);
    HANDLE old_handle = shm->handle;
    shm->id = new_id;
    shm_map_readwrite(shm, size);
    CloseHandle(old_handle);
}

void _bolt_plugin_shm_remap(struct BoltSHM* shm, size_t length, void* handle) {
    _bolt_plugin_shm_close(shm);
    shm->handle = (HANDLE)handle;
    shm->file = MapViewOfFile(shm->handle, FILE_MAP_READ, 0, 0, length);
}
