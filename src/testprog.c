#include <rddma_api.h>
#include <semaphore.h>
#include <pthread.h>
#include <tp_cmdline.h>

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
		unsigned long t_id = rddma_get_hex_arg(output,"mmap_offset");
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

struct source_handle {
	int (*f)(void *, char **, int *);
	void *h;
};

int get_inputs(void *s, char **command, int *size)
{
	struct gengetopt_args_info *opts = (struct gengetopt_args_info *)s;
	if (*size < opts->inputs_num)
		*command = opts->inputs[*size];
	return ((*size)++ < opts->inputs_num);
}

void *setup_inputs(struct gengetopt_args_info *opts)
{
	struct source_handle *h = malloc(sizeof(*h));
	h->f = get_inputs;
	h->h = (void *)opts;
	return (void *)h;
}

int get_file(void *s, char **command, int *size)
{
	FILE *fp = (FILE *)s;
	return fscanf(fp,"%a[^\n]\n",command) > 0;
}

void *setup_file(FILE *fp)
{
	struct source_handle *h = malloc(sizeof(*h));
	h->f = get_file;
	h->h = (void *)fp;
	return (void *)h;
}

int internal_cmd(struct rddma_dev *dev, char *cmd, int size)
{
	return 0;
}

int rddma_get_cmd(struct rddma_dev *dev, void *source, char **command, int *size)
{
	struct source_handle *src = (struct source_handle *)source;
	char *cmd;
	int ret;
	int sz;

	if (*command)
		free(*command);

	while ( (ret = src->f(src->h,&cmd,&sz)) > 0) {
		if (!internal_cmd(dev,cmd,sz))
			break;
	}

	*command = cmd;
	*size = sz;
	return ret;
}

int test_BO(void *h, struct gengetopt_args_info *opts)
{
	int i;
	struct rddma_dev *dev;
	char *output;
	char *cmd = NULL;
	int size = 0;
	int result = 1;

	printf("Blocking Ordered Mode\n");
	dev = rddma_open(NULL,opts->timeout_arg);
	while (rddma_get_cmd(dev,h,&cmd,&size) && result) {

		printf ("%s\n\t -> ",cmd);

		result = rddma_do_cmd(dev,&output, "%s\n", cmd);

		if (result < 0)
			break;

		sscanf(strstr(output,"result("),"result(%d)",&result);

		printf("%s\n",output);;

		free(output); 
	}

	rddma_close(dev);
	
	return result;
}

int test_NBIO(void *h, struct gengetopt_args_info *opts)
{
	int size = 0;
	char *cmd = NULL;
	struct rddma_dev *dev;
	char *output;
	int result = 1;

	printf("Non-Blocking Interleaved Ordered Mode\n");
	dev = rddma_open(NULL,opts->timeout_arg);

	while (rddma_get_cmd(dev,h,&cmd,&size) && result) {
		printf ("%s\n\t -> ",cmd);

		result = rddma_invoke_cmd(dev, "%s\n", cmd);
		if (result < 0)
			break;

		result = rddma_get_result(dev,&output);
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

	printf("\t -> %s\n",output);

	free(output); 
	rddma_free_async_handle(h);
}

int test_NBOO(void *h, struct gengetopt_args_info *opts)
{
	int i = 0;
	int count = 0;
	int size = 0;
	char *cmd = NULL;
	struct rddma_dev *dev;
	char *output;
	int result = 0;
	void *ah;
	pthread_t tid[100];

	printf("Non-Blocking Out of Order Mode\n");
	dev = rddma_open(NULL,opts->timeout_arg);

	while ( rddma_get_cmd(dev,h,&cmd,&size)) {

		printf ("%s\n",cmd);
		ah = rddma_alloc_async_handle();
		result = rddma_invoke_cmd(dev, "%s?request(%p)\n", cmd,ah);
		pthread_create(&tid[count++],0,thr_f,(void *)ah);
	}

	for (i = 0; i < count; i++) {
		result = rddma_get_result_async(dev);
	}

	for (i = 0; i < count; i++)
		pthread_join(tid[i],0);

	rddma_close(dev);

	return result;
}

int test_AIOE(void *h, struct gengetopt_args_info *opts)
{
	int i;
	int count = 0;
	int size = 0;
	char *cmd = NULL;
	struct rddma_dev *dev;
	char *output;
	int result = 0;
	void *ah;
	pthread_t tid[100];

	printf("Non-Blocking Out of Order Mode\n");
	dev = rddma_open(NULL,opts->timeout_arg);

	while (rddma_get_cmd(dev,h,&cmd,&size)) {
		printf ("%s\n",cmd);
		ah = rddma_alloc_async_handle();
		result = rddma_invoke_cmd(dev, "%s?request(%p)\n", cmd, ah);
		pthread_create(&tid[count++],0,thr_f,(void *)ah);
	}

	for (i = 0; i < count; i++) {
		result = rddma_get_result_async(dev);
	}

	for (i = 0; i < count; i++)
		pthread_join(tid[i],0);

	rddma_close(dev);
	
	return result;
}

int process_commands(void *h, struct gengetopt_args_info *opts)
{
	int result;
	switch(opts->mode_arg) {
	case mode_arg_BO:
		result = test_BO(h,opts);
		break;
	case mode_arg_NBIO:
		result = test_NBIO(h,opts);
		break;
	case mode_arg_NBOO:
		result = test_NBOO(h,opts);
		break;
	case mode_arg_AIOE:
		result = test_AIOE(h,opts);
		break;
	}
	return result;
}

int main (int argc, char **argv)
{
	struct gengetopt_args_info opts;
	void *h;

	cmdline_parser_init(&opts);

	cmdline_parser(argc,argv,&opts);

	if (opts.file_given) {
		h = setup_file(fopen(opts.file_arg,"r"));
		process_commands(h,&opts);
	}

	if (opts.inputs_num) {
		h = setup_inputs(&opts);
		process_commands(h,&opts);
	}

	if (opts.interactive_given) {
		h = setup_file(stdin);
		process_commands(h,&opts);
	}
}



