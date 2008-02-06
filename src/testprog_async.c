#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include "./aio_user.h"


void asyio_prep_pwrite(struct iocb *iocb, int fd, void const *buf, int nr_segs,
		       int64_t offset, int afd)
{
	memset(iocb, 0, sizeof(*iocb));
	iocb->aio_fildes = fd;
	iocb->aio_lio_opcode = IOCB_CMD_PWRITE;
	iocb->aio_reqprio = 0;
	iocb->aio_buf = (u_int64_t) (unsigned long) buf;
	iocb->aio_nbytes = nr_segs;
#if 0
	iocb->aio_offset = offset;
#else
	/* Address of string for rddma reply */
	iocb->aio_offset = (u_int64_t) (unsigned long) malloc(256);
#endif
	iocb->aio_flags = IOCB_FLAG_RESFD;
	iocb->aio_resfd = afd;
}


long waitasync(int afd, int timeo) {
	struct pollfd pfd;

	pfd.fd = afd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	if (poll(&pfd, 1, timeo) < 0) {
		perror("poll");
		return -1;
	}
	if ((pfd.revents & POLLIN) == 0) {
		fprintf(stderr, "no results completed\n");
		return 0;
	}

	return 1;
}

static unsigned long xtol (char *str);

int main (int argc, char **argv)
{
	int fd;
	int afd;
	FILE *file;
	struct iocb *iocb;
	static struct io_event events[1];
	aio_context_t ctx = 0;
	int r;
	struct timespec tmo;

	char *output;

	/* Initialize event fd */
	if ((afd = eventfd(0)) == -1) {
		perror("eventfd");
		return 2;
	}
	fcntl(afd, F_SETFL, fcntl(afd, F_GETFL, 0) | O_NONBLOCK);

	printf("Opening /dev/rddma...");
	fd = open("/dev/rddma",O_RDWR);
	if ( fd < 0 ) {
		perror("failed");
		return (0);
	}
	else
		printf("OK\n");

	/* Prepare asynchronous I/O request */
	iocb = malloc(sizeof(struct iocb));
	asyio_prep_pwrite(iocb, fd, argv[1], 1 + strlen(argv[1]),
		  0, afd);
	iocb->aio_data = (u_int64_t) 0x5354;  /* dummy value for now */

	/* Create context (for receiving events) 
	 * Only need ring space for 1 event, but allowing 2 
	 */
	if (io_setup(2, &ctx)) {
		perror("io_setup");
		return 3;
	}

	/* Do it */
	printf("Writing \"%s\" to /dev/rddma...",argv[1]);
	fflush(stdout);
	if (io_submit(ctx, 1, &iocb) <= 0) {
		perror("io_submit");
		return -1;
	}

	fprintf(stdout, "waiting ... ");
	fflush(stdout);
	/* second arg is wait time in msec, -1 is forever */
	waitasync(afd, -1);

	/* Done, get return value */
	tmo.tv_sec = 0;
	tmo.tv_nsec = 0;
	r = io_getevents(ctx, 1, 1, events, &tmo);
	if (r != 1)
		printf("Problem with io_getevents\n");

	output = (char *) (unsigned long)events[0].res2;
	printf("reply = %s\n", output);
	
	/*
	* If the request was "smb_mmap" then use the reply
	* to mmap the region we asked for.
	*/
	if (strcasestr (argv[1], "smb_mmap")) {
		/*
		* The reply ought to contain an "mmap_offset=<x>" term, 
		* where <x> is the offset, in hex, that we need to use
		* with actual mmap calls to map the target area.
		*/
		char *tid_s = strcasestr (output, "mmap_offset(");
		if (tid_s) {
			void* mapping;
			unsigned long t_id = xtol (tid_s + 12);
			printf ("mmap... %08lx\n", t_id);
			/*
			* Mmap the region and, for giggles, erase its
			* contents.
			*
			*/
			mapping = mmap (0, 512, PROT_READ | PROT_WRITE, MAP_SHARED, fd, t_id);
			printf ("mmaped to %p\n", mapping);
			if (mapping && (~((size_t)mapping))) memset (mapping, 0, 512);
		}
	}
	
 	free(output); 
 	free(iocb); 
	io_destroy(ctx);
	close(afd);
	close(fd);
	
}

/**
* xtol - convert hex string to long integer
*
* @str: string to convert.
*
* This function converts hex digits at a specified string into an
* unsigned long using the perfectly useful strtol
*
**/
static unsigned long xtol (char *str)
{
	return strtol(str,0,16);
}


