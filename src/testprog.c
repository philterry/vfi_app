#include <rddma_api.h>
#include <semaphore.h>
#include <pthread.h>
#include "cmdline.h"

void fred(struct rddma_dev *dev, char *input, char *output)
{

	/*
	* If the request was "smb_mmap" then use the reply
	* to mmap the region we asked for.
	*/
	if (strcasestr (input, "smb_mmap://")) {
		/*
		* The reply ought to contain an "mmap_offset=<x>" term, 
		* where <x> is the offset, in hex, that we need to use
		* with actual mmap calls to map the target area.
		*/
		unsigned long t_id = rddma_get_hex_option(output,"mmap_offset");
		if (t_id) {
			void* mapping;
			/*
			* Mmap the region and, for giggles, erase its
			* contents.
			*
			*/
			mapping = mmap (0, 512, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, t_id);
			if ((unsigned long) mapping == -1) {
				perror("mmap failed");
			}
			else {
				printf ("mmaped to %p\n", mapping);
				printf("initialize first 512 bytes\n");
				memset (mapping, 0, 512);
			}
		}
		else {
			printf("smb_mmap failed\n");
		}
	}
}

int test_BO(int np, char **p)
{
	int i;
	struct rddma_dev *dev;
	char *output;
	int result = 1;

	printf("Blocking Ordered Mode\n");
	dev = rddma_open(NULL,0);

	for (i = 0; i < np && result; i++) {
		printf ("%s\n\t -> ",p[i]);

		result = rddma_do_cmd(dev,&output, "%s\n", p[i]);

		if (result < 0)
			break;

		sscanf(strstr(output,"result("),"result(%d)",&result);

		printf("%s\n",output);;

		free(output); 
	}

	rddma_close(dev);
	
	return result;
}

int test_NBO(int np, char **p)
{
	int i;
	struct rddma_dev *dev;
	char *output;
	int result = 1;
	int timeout = -1;

	printf("Non-Blocking Ordered Mode\n");
	dev = rddma_open(NULL,O_NONBLOCK | O_RDWR);

	for (i = 0; i < np && result; i++) {
		printf ("inputs[%d]: %s\n\t -> ",i,p[i]);

		result = rddma_do_cmd_blk(dev,timeout,&output, "%s\n", p[i]);
		if (result < 0)
			break;

		sscanf(strstr(output,"result("),"result(%d)",&result);

		printf("%s\n",output);

		free(output); 
	}

	rddma_close(dev);
	
	return result;
}

int test_NBIO(int np, char **p)
{
	int i;
	struct rddma_dev *dev;
	char *output;
	int result = 1;
	int timeout = -1;
	int count = 0;

	printf("Non-Blocking Interleaved Ordered Mode\n");
	dev = rddma_open(NULL,O_NONBLOCK | O_RDWR);

	for (i = 0; i < np && result; i++) {
		printf ("inputs[%d]: %s\n ",i,p[i]);

		result = rddma_invoke_cmd(dev, "%s\n", p[i]);
		if (result < 0)
			break;
		count++;
	}
	result = 1;
	for (i = 0; i < count && result; i++) {

		result = rddma_get_result(dev,timeout,&output);
		if (result < 0)
			break;

		sscanf(strstr(output,"result("),"result(%d)",&result);

		printf("%s\n",output);

		free(output); 
	}

	rddma_close(dev);
	
	return result;
}

void *thr_f(void *h)
{
	char *output;
	int result;

	result = rddma_get_async_handle(h,&output);
	
	sscanf(strstr(output,"result("),"result(%d)",&result);

	printf("%s\n",output);

	free(output); 
	rddma_free_async_handle(h);
}

int test_NBOO(int np, char **p)
{
	int i;
	struct rddma_dev *dev;
	char *output;
	int result = 0;
	int timeout = -1;
	void *h;
	pthread_t tid;

	printf("Non-Blocking Out of Order Mode\n");
	dev = rddma_open(NULL,O_NONBLOCK | O_RDWR);

	for (i = 0; i < np; i++) {
		printf ("inputs[%d]: %s\n ",i,p[i]);
		h = rddma_alloc_async_handle();
		result = rddma_invoke_cmd(dev, "%s?request(%p)\n", p[i],h);
		pthread_create(&tid,0,thr_f,(void *)h);
	}

	for (i = 0; i < np; i++) {
		result = rddma_get_result_async(dev,timeout);
	}

	rddma_close(dev);
	
	return result;
}

int main (int argc, char **argv)
{
	int result;
	int i;
	char *output;
	struct rddma_dev *dev;
	struct gengetopt_args_info opts;

	cmdline_parser_init(&opts);

	cmdline_parser(argc,argv,&opts);

	switch(opts.mode_arg) {
	case mode_arg_BO:
		result = test_BO(opts.inputs_num,opts.inputs);
		break;
	case mode_arg_NBO:
		result = test_NBO(opts.inputs_num,opts.inputs);
		break;
	case mode_arg_NBIO:
		result = test_NBIO(opts.inputs_num,opts.inputs);
		break;
	case mode_arg_NBOO:
		result = test_NBOO(opts.inputs_num,opts.inputs);
		break;
	}
	
	return result;

}



