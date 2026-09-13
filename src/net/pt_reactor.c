#include "net/pt_reactor.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(PMT_USE_KQUEUE)
#include <sys/event.h>
#include <sys/time.h>

typedef struct {
    pt_io_cb_t cb;
    void      *ud;
    int        events;
} pt_kqueue_slot_t;

#define PT_MAX_FDS 4096
static pt_kqueue_slot_t s_kslots[PT_MAX_FDS];

int pt_reactor_init(pt_reactor_t *r)
{
    if (!r) return -1;
    r->poll_fd = kqueue();
    r->running = (r->poll_fd >= 0);
    return r->running ? 0 : -1;
}

void pt_reactor_destroy(pt_reactor_t *r)
{
    if (r && r->poll_fd >= 0) {
        close(r->poll_fd);
        r->poll_fd = -1;
    }
}

int pt_reactor_add(pt_reactor_t *r, int fd, int events, pt_io_cb_t cb, void *ud)
{
    if (!r || fd < 0 || fd >= PT_MAX_FDS) return -1;
    s_kslots[fd].cb = cb;
    s_kslots[fd].ud = ud;
    s_kslots[fd].events = events;

    struct kevent ch[2];
    int n = 0;
    if (events & PT_EV_READ)  EV_SET(&ch[n++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*)(uintptr_t)fd);
    if (events & PT_EV_WRITE) EV_SET(&ch[n++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void*)(uintptr_t)fd);
    return kevent(r->poll_fd, ch, n, NULL, 0, NULL);
}

int pt_reactor_del(pt_reactor_t *r, int fd)
{
    if (!r || fd < 0 || fd >= PT_MAX_FDS) return -1;
    struct kevent ch[2];
    EV_SET(&ch[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&ch[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(r->poll_fd, ch, 2, NULL, 0, NULL);
    s_kslots[fd].cb = NULL;
    return 0;
}

int pt_reactor_poll(pt_reactor_t *r, int timeout_ms)
{
    if (!r || r->poll_fd < 0) return -1;
    struct kevent evs[64];
    struct timespec ts;
    struct timespec *tsp = NULL;
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        tsp = &ts;
    }
    int nev = kevent(r->poll_fd, NULL, 0, evs, 64, tsp);
    for (int i = 0; i < nev; i++) {
        int fd = (int)(uintptr_t)evs[i].udata;
        if (fd >= 0 && fd < PT_MAX_FDS && s_kslots[fd].cb) {
            int mask = 0;
            if (evs[i].filter == EVFILT_READ)  mask |= PT_EV_READ;
            if (evs[i].filter == EVFILT_WRITE) mask |= PT_EV_WRITE;
            if (evs[i].flags & EV_ERROR)       mask |= PT_EV_ERROR;
            s_kslots[fd].cb(fd, mask, s_kslots[fd].ud);
        }
    }
    return nev;
}

void pt_reactor_stop(pt_reactor_t *r) { if (r) r->running = 0; }

#elif defined(PMT_USE_EPOLL)
#include <sys/epoll.h>

typedef struct {
    pt_io_cb_t cb;
    void      *ud;
} pt_epoll_slot_t;

#define PT_MAX_FDS 4096
static pt_epoll_slot_t s_eslots[PT_MAX_FDS];

int pt_reactor_init(pt_reactor_t *r)
{
    if (!r) return -1;
    r->poll_fd = epoll_create1(0);
    r->running = (r->poll_fd >= 0);
    return r->running ? 0 : -1;
}

void pt_reactor_destroy(pt_reactor_t *r)
{
    if (r && r->poll_fd >= 0) { close(r->poll_fd); r->poll_fd = -1; }
}

int pt_reactor_add(pt_reactor_t *r, int fd, int events, pt_io_cb_t cb, void *ud)
{
    if (!r || fd < 0 || fd >= PT_MAX_FDS) return -1;
    s_eslots[fd].cb = cb;
    s_eslots[fd].ud = ud;
    struct epoll_event ev;
    ev.events = 0;
    if (events & PT_EV_READ)  ev.events |= EPOLLIN;
    if (events & PT_EV_WRITE) ev.events |= EPOLLOUT;
    ev.data.fd = fd;
    return epoll_ctl(r->poll_fd, EPOLL_CTL_ADD, fd, &ev);
}

int pt_reactor_del(pt_reactor_t *r, int fd)
{
    if (!r || fd < 0 || fd >= PT_MAX_FDS) return -1;
    s_eslots[fd].cb = NULL;
    return epoll_ctl(r->poll_fd, EPOLL_CTL_DEL, fd, NULL);
}

int pt_reactor_poll(pt_reactor_t *r, int timeout_ms)
{
    if (!r || r->poll_fd < 0) return -1;
    struct epoll_event evs[64];
    int nev = epoll_wait(r->poll_fd, evs, 64, timeout_ms);
    for (int i = 0; i < nev; i++) {
        int fd = evs[i].data.fd;
        if (fd >= 0 && fd < PT_MAX_FDS && s_eslots[fd].cb) {
            int mask = 0;
            if (evs[i].events & EPOLLIN)  mask |= PT_EV_READ;
            if (evs[i].events & EPOLLOUT) mask |= PT_EV_WRITE;
            if (evs[i].events & (EPOLLERR | EPOLLHUP)) mask |= PT_EV_ERROR;
            s_eslots[fd].cb(fd, mask, s_eslots[fd].ud);
        }
    }
    return nev;
}

void pt_reactor_stop(pt_reactor_t *r) { if (r) r->running = 0; }

#endif