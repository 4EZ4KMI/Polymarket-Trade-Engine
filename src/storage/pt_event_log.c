#include "storage/pt_event_log.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int pt_mmap_log_open(pt_mmap_log_t *l, const char *path, size_t capacity_bytes)
{
    if (l == NULL || path == NULL || capacity_bytes == 0) return -1;
    memset(l, 0, sizeof(*l));
    strncpy(l->filepath, path, sizeof(l->filepath) - 1);
    l->capacity_bytes = capacity_bytes;

    l->fd = open(path, O_RDWR | O_CREAT, 0644);
    if (l->fd < 0) return -1;

    struct stat st;
    if (fstat(l->fd, &st) == 0 && (size_t)st.st_size < capacity_bytes) {
        if (ftruncate(l->fd, capacity_bytes) != 0) {
            close(l->fd);
            return -1;
        }
    }

    l->mmap_base = mmap(NULL, capacity_bytes, PROT_READ | PROT_WRITE,
                        MAP_SHARED, l->fd, 0);
    if (l->mmap_base == MAP_FAILED) {
        close(l->fd);
        l->mmap_base = NULL;
        return -1;
    }
    l->write_offset = 0;
    return 0;
}

void pt_mmap_log_close(pt_mmap_log_t *l)
{
    if (l == NULL) return;
    if (l->mmap_base && l->mmap_base != MAP_FAILED) {
        msync(l->mmap_base, l->capacity_bytes, MS_SYNC);
        munmap(l->mmap_base, l->capacity_bytes);
    }
    if (l->fd >= 0) close(l->fd);
    memset(l, 0, sizeof(*l));
}

int pt_mmap_log_append(pt_mmap_log_t *l, const pt_raw_event_t *ev)
{
    if (l == NULL || l->mmap_base == NULL || ev == NULL) return -1;
    size_t sz = sizeof(pt_raw_event_t);
    if (l->write_offset + sz > l->capacity_bytes) return -1; /* full */

    uint8_t *dst = (uint8_t *)l->mmap_base + l->write_offset;
    memcpy(dst, ev, sz);
    l->write_offset += sz;
    return 0;
}

void pt_mmap_log_flush(pt_mmap_log_t *l)
{
    if (l && l->mmap_base && l->mmap_base != MAP_FAILED) {
        msync(l->mmap_base, l->write_offset, MS_ASYNC);
    }
}