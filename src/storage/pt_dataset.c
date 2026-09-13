#include "storage/pt_dataset.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int pt_dataset_writer_open(pt_dataset_writer_t *w, const char *path)
{
    if (!w || !path) return -1;
    memset(w, 0, sizeof(*w));
    strncpy(w->filepath, path, sizeof(w->filepath) - 1);
    w->fp = fopen(path, "wb");
    if (!w->fp) return -1;

    w->header.magic = PMT_DATASET_MAGIC;
    w->header.version = PMT_DATASET_VERSION;
    w->header.total_events = 0;
    fwrite(&w->header, sizeof(pt_dataset_header_t), 1, w->fp);
    return 0;
}

int pt_dataset_writer_append(pt_dataset_writer_t *w, const pt_dataset_event_t *ev)
{
    if (!w || !w->fp || !ev) return -1;
    if (w->header.total_events == 0) {
        w->header.start_ts_ns = ev->timestamp_ns;
    }
    w->header.end_ts_ns = ev->timestamp_ns;
    w->header.total_events++;
    return fwrite(ev, sizeof(pt_dataset_event_t), 1, w->fp) == 1 ? 0 : -1;
}

void pt_dataset_writer_close(pt_dataset_writer_t *w)
{
    if (!w || !w->fp) return;
    fseek(w->fp, 0, SEEK_SET);
    fwrite(&w->header, sizeof(pt_dataset_header_t), 1, w->fp);
    fclose(w->fp);
    w->fp = NULL;
}

int pt_dataset_reader_open(pt_dataset_reader_t *r, const char *path)
{
    if (!r || !path) return -1;
    memset(r, 0, sizeof(*r));
    r->fd = open(path, O_RDONLY);
    if (r->fd < 0) return -1;

    struct stat st;
    if (fstat(r->fd, &st) != 0 || (size_t)st.st_size < sizeof(pt_dataset_header_t)) {
        close(r->fd);
        return -1;
    }
    r->file_size = (size_t)st.st_size;
    r->mmap_base = mmap(NULL, r->file_size, PROT_READ, MAP_SHARED, r->fd, 0);
    if (r->mmap_base == MAP_FAILED) {
        close(r->fd);
        r->mmap_base = NULL;
        return -1;
    }

    memcpy(&r->header, r->mmap_base, sizeof(pt_dataset_header_t));
    if (r->header.magic != PMT_DATASET_MAGIC || r->header.version != PMT_DATASET_VERSION) {
        pt_dataset_reader_close(r);
        return -1;
    }
    r->current_idx = 0;
    return 0;
}

int pt_dataset_reader_next(pt_dataset_reader_t *r, pt_dataset_event_t *ev_out)
{
    if (!r || !r->mmap_base || !ev_out) return 0;
    if (r->current_idx >= r->header.total_events) return 0;

    size_t offset = sizeof(pt_dataset_header_t) + (size_t)r->current_idx * sizeof(pt_dataset_event_t);
    if (offset + sizeof(pt_dataset_event_t) > r->file_size) return 0;

    const uint8_t *src = (const uint8_t *)r->mmap_base + offset;
    memcpy(ev_out, src, sizeof(pt_dataset_event_t));
    r->current_idx++;
    return 1;
}

void pt_dataset_reader_rewind(pt_dataset_reader_t *r)
{
    if (r) r->current_idx = 0;
}

void pt_dataset_reader_close(pt_dataset_reader_t *r)
{
    if (!r) return;
    if (r->mmap_base && r->mmap_base != MAP_FAILED) {
        munmap(r->mmap_base, r->file_size);
        r->mmap_base = NULL;
    }
    if (r->fd >= 0) {
        close(r->fd);
        r->fd = -1;
    }
}
