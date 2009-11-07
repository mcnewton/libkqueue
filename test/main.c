/*
 * Copyright (c) 2009 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include "common.h"

char *cur_test_id = "undef";
int kqfd;

extern void test_evfilt_read();
extern void test_evfilt_signal();
extern void test_evfilt_vnode();
extern void test_evfilt_timer();

/* Checks if any events are pending, which is an error. */
void 
test_no_kevents(void)
{
    int nfds;
    struct timespec timeo;
    struct kevent kev;

    puts("confirming that there are no events pending");
    memset(&timeo, 0, sizeof(timeo));
    nfds = kevent(kqfd, NULL, 0, &kev, 1, &timeo);
    if (nfds != 0) {
        puts("\nUnexpected event:");
        puts(kevent_to_str(&kev));
        errx(1, "%d event(s) pending, but none expected:", nfds);
    }
}

/* Retrieve a single kevent */
struct kevent *
kevent_get(int kqfd)
{
    int nfds;
    struct kevent *kev;

    if ((kev = calloc(1, sizeof(*kev))) == NULL)
	err(1, "out of memory");
    
    nfds = kevent(kqfd, NULL, 0, kev, 1, NULL);
    if (nfds < 1)
        err(1, "kevent(2)");

    return (kev);
}

char *
kevent_fflags_dump(struct kevent *kev)
{
    char *buf;

#define KEVFFL_DUMP(attrib) \
    if (kev->fflags & attrib) \
	strcat(buf, #attrib" ");

    if ((buf = calloc(1, 1024)) == NULL)
	abort();

    /* Not every filter has meaningful fflags */
    if (kev->filter != EVFILT_VNODE) {
    	sprintf(buf, "fflags = %d", kev->flags);
	return (buf);
    }

    sprintf(buf, "fflags = %d (", kev->fflags);
    KEVFFL_DUMP(NOTE_DELETE);
    KEVFFL_DUMP(NOTE_WRITE);
    KEVFFL_DUMP(NOTE_EXTEND);
#if HAVE_NOTE_TRUNCATE
    KEVFFL_DUMP(NOTE_TRUNCATE);
#endif
    KEVFFL_DUMP(NOTE_ATTRIB);
    KEVFFL_DUMP(NOTE_LINK);
    KEVFFL_DUMP(NOTE_RENAME);
#if HAVE_NOTE_REVOKE
    KEVFFL_DUMP(NOTE_REVOKE);
#endif
    buf[strlen(buf) - 1] = ')';

    return (buf);
}

char *
kevent_flags_dump(struct kevent *kev)
{
    char *buf;

#define KEVFL_DUMP(attrib) \
    if (kev->flags & attrib) \
	strcat(buf, #attrib" ");

    if ((buf = calloc(1, 1024)) == NULL)
	abort();

    sprintf(buf, "flags = %d (", kev->flags);
    KEVFL_DUMP(EV_ADD);
    KEVFL_DUMP(EV_ENABLE);
    KEVFL_DUMP(EV_DISABLE);
    KEVFL_DUMP(EV_DELETE);
    KEVFL_DUMP(EV_ONESHOT);
    KEVFL_DUMP(EV_CLEAR);
    KEVFL_DUMP(EV_EOF);
    KEVFL_DUMP(EV_ERROR);
#if HAVE_EV_DISPATCH
    KEVFL_DUMP(EV_DISPATCH);
#endif
#if HAVE_EV_RECEIPT
    KEVFL_DUMP(EV_RECEIPT);
#endif
    buf[strlen(buf) - 1] = ')';

    return (buf);
}

/* Copied from ../kevent.c kevent_dump() and improved */
const char *
kevent_to_str(struct kevent *kev)
{
    char buf[512];
    snprintf(&buf[0], sizeof(buf), "[filter=%d,%s,%s,ident=%u,data=%d,udata=%p]",
            kev->filter,
            kevent_flags_dump(kev),
            kevent_fflags_dump(kev),
            (u_int) kev->ident,
            (int) kev->data,
            kev->udata);
    return (strdup(buf));
}

void
kevent_cmp(struct kevent *k1, struct kevent *k2)
{
    if (memcmp(k1, k2, sizeof(*k1)) != 0) {
        printf("kevent_cmp: mismatch:\n  %s !=\n  %s\n", 
              kevent_to_str(k1), kevent_to_str(k2));
        abort();
    }
}

void
test_begin(const char *func)
{
    static int testnum = 1;
    cur_test_id = (char *) func;
    printf("\n\nTest %d: %s\n", testnum++, func);
}

void
success(const char *func)
{
    printf("%-70s %s\n", func, "passed");
}

void
test_kqueue(void)
{
    test_begin("kqueue()");
    if ((kqfd = kqueue()) < 0)
        err(1, "kqueue()");
    test_no_kevents();
    success("kqueue()");
}

void
test_kqueue_close(void)
{
    test_begin("close(kq)");
    if (close(kqfd) < 0)
        err(1, "close()");
#if LIBKQUEUE
    kqueue_free(kqfd);
#endif
    success("kqueue_close()");
}

int 
main(int argc, char **argv)
{
    int test_socket = 1;
    int test_signal = 1;//XXX-FIXME
    int test_vnode = 1;
    int test_timer = 1;

    while (argc) {
        if (strcmp(argv[0], "--no-socket") == 0)
            test_socket = 0;
        if (strcmp(argv[0], "--no-timer") == 0)
            test_timer = 0;
        if (strcmp(argv[0], "--no-signal") == 0)
            test_signal = 0;
        if (strcmp(argv[0], "--no-vnode") == 0)
            test_vnode = 0;
        argv++;
        argc--;
    }

    test_kqueue();
    test_kqueue_close();

    if (test_socket) 
        test_evfilt_read();
    if (test_signal) 
        test_evfilt_signal();
    if (test_vnode) 
        test_evfilt_vnode();
    if (test_timer) 
        test_evfilt_timer();

    puts("all tests completed.");
    return (0);
}