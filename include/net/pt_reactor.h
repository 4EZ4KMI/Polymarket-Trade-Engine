#ifndef PMT_PT_REACTOR_H
#define PMT_PT_REACTOR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PT_EV_READ  = 1 << 0,
    PT_EV_WRITE = 1 << 1,
    PT_EV_ERROR = 1 << 2
} pt_io_events_t;

typedef void (*pt_io_cb_t)(int fd, int events, void *userdata);

typedef struct {
    int  poll_fd;    /* kqueue or epoll fd */
    int  running;
} pt_reactor_t;

int  pt_reactor_init(pt_reactor_t *r);
void pt_reactor_destroy(pt_reactor_t *r);

/* Register / modify interest on an fd */
int  pt_reactor_add(pt_reactor_t *r, int fd, int events, pt_io_cb_t cb, void *ud);
int  pt_reactor_del(pt_reactor_t *r, int fd);

/* Run one iteration of event polling with timeout_ms (-1 = block) */
int  pt_reactor_poll(pt_reactor_t *r, int timeout_ms);
void pt_reactor_stop(pt_reactor_t *r);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_REACTOR_H */