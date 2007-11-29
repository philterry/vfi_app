#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>

 /*
 * This were good at the time of 2.6.21-rc5.mm4 ...
 */
#ifndef __NR_eventfd
#if defined(__x86_64__)
#define __NR_eventfd 283
#elif defined(__i386__)
#define __NR_eventfd 323
#elif defined(__powerpc__)
#define __NR_eventfd 308
#else
#error Cannot detect your architecture!
#endif
#endif

typedef unsigned long aio_context_t;

enum {
	IOCB_CMD_PREAD = 0,
		IOCB_CMD_PWRITE = 1,
		IOCB_CMD_FSYNC = 2,
		IOCB_CMD_FDSYNC = 3,
		/* These two are experimental.
		 * IOCB_CMD_PREADX = 4,
		 * IOCB_CMD_POLL = 5,
		 */
		IOCB_CMD_NOOP = 6,
		IOCB_CMD_PREADV = 7,
		IOCB_CMD_PWRITEV = 8,
};

#if defined(__LITTLE_ENDIAN)
#define PADDED(x,y)	x, y
#elif defined(__BIG_ENDIAN)
#define PADDED(x,y)	y, x
#else
#error edit for your odd byteorder.
#endif

#define IOCB_FLAG_RESFD		(1 << 0)

/*
 * we always use a 64bit off_t when communicating
 * with userland.  its up to libraries to do the
 * proper padding and aio_error abstraction
 */
struct iocb {
	/* these are internal to the kernel/libc. */
	u_int64_t	aio_data;	/* data to be returned in event's data */
	u_int32_t	PADDED(aio_key, aio_reserved1);
	/* the kernel sets aio_key to the req # */

	/* common fields */
	u_int16_t	aio_lio_opcode;	/* see IOCB_CMD_ above */
	int16_t	aio_reqprio;
	u_int32_t	aio_fildes;

	u_int64_t	aio_buf;
	u_int64_t	aio_nbytes;
	int64_t	aio_offset;

	/* extra parameters */
	u_int64_t	aio_reserved2;	/* TODO: use this for a (struct sigevent *) */

	u_int32_t	aio_flags;
	/*
	 * If different from 0, this is an eventfd to deliver AIO results to
	 */
	u_int32_t	aio_resfd;
}; /* 64 bytes */


struct io_event {
	u_int64_t           data;           /* the data field from the iocb */
	u_int64_t           obj;            /* what iocb this event came from */
	int64_t           res;            /* result code for this event */
	int64_t           res2;           /* secondary result */
};

static inline long io_setup(unsigned nr_reqs, aio_context_t *ctx) {

	return syscall(__NR_io_setup, nr_reqs, ctx);
}

static inline long io_destroy(aio_context_t ctx) {

	return syscall(__NR_io_destroy, ctx);
}

static inline long io_submit(aio_context_t ctx, long n, struct iocb **paiocb) {

	return syscall(__NR_io_submit, ctx, n, paiocb);
}

static inline long io_cancel(aio_context_t ctx, struct iocb *aiocb, struct io_event *res) {

	return syscall(__NR_io_cancel, ctx, aiocb, res);
}

static inline long io_getevents(aio_context_t ctx, long min_nr, long nr, struct io_event *events,
		  struct timespec *tmo) {

	return syscall(__NR_io_getevents, ctx, min_nr, nr, events, tmo);
}

static inline int eventfd(int count) {

	return syscall(__NR_eventfd, count);
}
