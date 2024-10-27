#define _GNU_SOURCE 1
#include <unistd.h>
#include <sys/mman.h>

#include "plugin.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

uint8_t _bolt_plugin_shm_open_inbound(struct BoltSHM* shm, const char* tag, uint64_t id) {
    char buf[256];
    snprintf(buf, sizeof(buf), "/bolt-%i-%s-%lu", getpid(), tag, id);
    shm->fd = shm_open(buf, O_RDONLY | O_CREAT | O_TRUNC, 0644);
    if (shm->fd == -1) {
        printf("failed to create shm object '%s': %i\n", buf, errno);
        return 0;
    }
    shm->file = NULL;
    shm->unlink_pid = 0;
    shm->tag = tag;
    shm->id = id;
    return 1;
}

uint8_t _bolt_plugin_shm_open_outbound(struct BoltSHM* shm, size_t size, const char* tag, uint64_t id) {
    char buf[256];
    shm->unlink_pid = getpid();
    snprintf(buf, sizeof(buf), "/bolt-%i-%s-%lu", shm->unlink_pid, tag, id);
    shm->fd = shm_open(buf, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (shm->fd == -1) {
        printf("failed to create shm object '%s': %i\n", buf, errno);
        return 0;
    }
    if (ftruncate(shm->fd, size)) {
        printf("failed to truncate shm object to size %llu: %i\n", (unsigned long long)size, errno);
        close(shm->fd);
        return 0;
    }
    shm->file = mmap(NULL, size, PROT_WRITE, MAP_SHARED, shm->fd, 0);
    shm->tag = tag;
    shm->id = id;
    return 1;
}

void _bolt_plugin_shm_close(struct BoltSHM* shm) {
    if (shm->file) munmap(shm->file, shm->map_length);
    close(shm->fd);
    if (shm->unlink_pid > 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "/bolt-%i-%s-%lu", shm->unlink_pid, shm->tag, shm->id);
        shm_unlink(buf);
    }
}

void _bolt_plugin_shm_resize(struct BoltSHM* shm, size_t length, uint64_t new_id) {
    if (ftruncate(shm->fd, length)) {
        printf("failed to truncate shm object to size %llu: %i\n", (unsigned long long)length, errno);
        return;
    }
    shm->file = mremap(shm->file, shm->map_length, length, MREMAP_MAYMOVE);
    shm->map_length = length;
}

void _bolt_plugin_shm_remap(struct BoltSHM* shm, size_t length, void* handle) {
    if (!shm->file) {
        shm->file = mmap(NULL, length, PROT_READ, MAP_SHARED, shm->fd, 0);
    } else {
        shm->file = mremap(shm->file, shm->map_length, length, MREMAP_MAYMOVE);
    }
    shm->map_length = length;
}
