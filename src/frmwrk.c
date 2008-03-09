#include <rddma_api.h>
#include <semaphore.h>
#include <pthread.h>
#include <fw_cmdline.h>

int get_inputs(void *s, char **command)
{
	struct gengetopt_args_info *opts = (struct gengetopt_args_info *)s;
	*command = *opts->inputs++;
	return opts->inputs_num--;
}

int setup_inputs(struct rddma_source **src, struct gengetopt_args_info *opts)
{
	struct rddma_source *h = malloc(sizeof(*h));
	*src = h;
	if (src == NULL)
		return -ENOMEM;

	h->f = get_inputs;
	h->h = (void *)opts;
	return 0;
}

void *source_thread(void *source)
{
	char *cmd = NULL;
	struct rddma_source *src = (struct rddma_source *)source;
	while (rddma_get_cmd(src,&cmd))
		printf("%s:%s\n",__FUNCTION__,cmd);
	printf("%s:I'mdone\n",__FUNCTION__);
	return 0;
}

int process_commands(pthread_t *tid, struct rddma_source *src, struct gengetopt_args_info *opts)
{
	return pthread_create(tid,0,source_thread,(void *)src);
}

static int done = 0;

void *driver_thread(void *h)
{
	struct rddma_dev *dev = (struct rddma_dev *)h;
	while (!done)
		rddma_put_async_handle(dev);
}

int main (int argc, char **argv)
{
	struct gengetopt_args_info opts;
	struct rddma_dev *dev;
	struct rddma_source *src;

	pthread_t driver_tid;

	pthread_t file_tid;
	pthread_t opts_tid;
	pthread_t user_tid;

	int ft = 0;
	int ot = 0;
	int ut = 0;

	cmdline_parser_init(&opts);
	cmdline_parser(argc,argv,&opts);

	rddma_open(&dev,opts.device_arg,opts.timeout_arg);
	
	pthread_create(&driver_tid,0,driver_thread,(void *)dev);

	while (opts.file_given--) {
		FILE *file = fopen(*opts.file_arg++,"r");
		if (!rddma_setup_file(&src,file))
			ft = process_commands(&file_tid, src, &opts) == 0;
	}

	if (opts.inputs_num) {
		if (!setup_inputs(&src,&opts))
			ot = process_commands(&opts_tid, src, &opts) == 0;
	}

	if (opts.interactive_given) {
		if (!rddma_setup_file(&src,stdin))
			ut =process_commands(&user_tid, src, &opts) == 0;
	}

	if (ft)	pthread_join(file_tid,0);
	if (ot) pthread_join(opts_tid,0);
	if (ut) pthread_join(user_tid,0);

	done = 1;
	pthread_join(driver_tid,NULL);
	return 0;
}
