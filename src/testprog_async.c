#include <rddma_api.h>

int main (int argc, char **argv)
{
	int afd;
	struct rddma_dev *dev;
	struct iocb *iocb;
	static struct io_event events[1];
	aio_context_t ctx = 0;
	int r;
	struct timespec tmo;

	char *output;

	/* Initialize event fd */
	if ((afd = rddma_get_eventfd(0)) < 0) {
		perror("eventfd");
		return 2;
	}

	dev = rddma_open(NULL,0);

	/* Prepare asynchronous I/O request */
	iocb = malloc(sizeof(struct iocb));
	asyio_prep_pwrite(iocb, dev->fd, argv[1], 1 + strlen(argv[1]),
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
	if (io_submit(ctx, 1, &iocb) <= 0) {
		perror("io_submit");
		return -1;
	}

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
	if (strstr (argv[1], "smb_mmap://")) {
		/*
		* The reply ought to contain an "mmap_offset=<x>" term, 
		* where <x> is the offset, in hex, that we need to use
		* with actual mmap calls to map the target area.
		*/
		char *tid_s = strstr (output, "mmap_offset(");
		if (tid_s) {
			void* mapping;
			unsigned long t_id = strtol (tid_s + 12,0,16);
			printf ("mmap... %08lx\n", t_id);
			/*
			* Mmap the region and, for giggles, erase its
			* contents.
			*
			*/
			mapping = mmap (0, 512, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, t_id);
			printf ("mmaped to %p\n", mapping);
			if (mapping && (~((size_t)mapping))) memset (mapping, 0, 512);
		}
	}
	
 	free(output); 
 	free(iocb); 
	io_destroy(ctx);
	close(afd);
	rddma_close(dev);
	
}

