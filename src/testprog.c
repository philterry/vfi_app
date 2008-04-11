#include <vfi_api.h>
#include <semaphore.h>
#include <pthread.h>
#include <tp_cmdline.h>

/*
 * The following is iffy, pseudo-code to help thrash out the API design.
 */


/* An example of application processing style, in this case Blocking Ordered */
int test_BO(struct vfi_dev *dev, struct vfi_source *src, struct gengetopt_args_info *opts)
{
	char *output;
	char *cmd = NULL;
	int rc = 0;
	long result;
	printf("Blocking Ordered Mode\n");
	while (vfi_get_cmd(src,&cmd) && !rc) {


		printf ("%s\n\t -> ",cmd);

		rc = vfi_do_cmd(dev,&output, "%s\n", cmd);

		if (rc < 0)
			break;

		rc = vfi_get_dec_arg(output,"result", &result);

		printf("%s\n",output);;

		free(output); 
	}

	return rc;
}

/* Another example of application processing style, overlapped driver
 * command with processing but block at end of processing before next
 * driver invocation, ie Blocked Interleaved Ordered. */
int test_NBIO(struct vfi_dev *dev, struct vfi_source *src, struct gengetopt_args_info *opts)
{
	char *cmd = NULL;
	char *output;
	long result;
	int rc = 0;

	printf("Non-Blocking Interleaved Ordered Mode\n");

	while (vfi_get_cmd(src,&cmd) && !rc) {
		printf ("%s\n\t -> ",cmd);

		rc = vfi_invoke_cmd(dev, "%s\n", cmd);
		if (rc < 0)
			break;

		rc = vfi_get_result(dev,&output);
		if (rc < 0)
			break;

		rc = vfi_get_dec_arg(output,"result", &result);

		printf("%s\n",output);

		free(output);
	}

	return rc;
}

/* These are just example threads... */
void *thr_f(void *h)
{
	char *output;
	int result;
	void **e;
	int done = 0;

	while (!done) {
		vfi_wait_async_handle(h,&output,(void **)&e);
		if (e)
			done = ((int (*)(void **,char *))(e[0]))(e,output);
		else
			done = 1;
	}

	printf("\t -> %s\n",output);
	fflush(stdout);

	free(output); 
	vfi_free_async_handle(h);
	pthread_exit(0);
}

/* This is the main dispatcher loop thread started by main before
 * spawning dependent threads above... */
void *thr_l(void *h)
{
	struct vfi_dev *dev = (struct vfi_dev *)h;
	while (1)
		vfi_post_async_handle(dev);
}

/* Another example of application processing style, completely out of
 * order overlapped processing with driver invocations using
 * applicaton threads. */
int test_NBOO(struct vfi_dev *dev,struct vfi_source *src, struct gengetopt_args_info *opts)
{
	int i = 0;
	int count = 0;
	char *cmd = NULL;

	int result = 0;
	void *ah;
	pthread_t tid[100];
	pthread_t clean;
	void **e = NULL;

	printf("Non-Blocking Out of Order Mode\n"); 

	pthread_create(&clean,0,thr_l,(void *)dev);

	while ( vfi_get_cmd(src,&cmd)) {
		printf ("%s\n",cmd);
		ah = vfi_alloc_async_handle(NULL);
		result = vfi_invoke_cmd(dev, "%s?request(%p)\n", cmd,ah);
		pthread_create(&tid[count++],0,thr_f,(void *)ah);
	}

	for (i = 0; i < count; i++)
		pthread_join(tid[i],0);

	return result;
}

/* Another application processing style example, same as NBOO but
 * implemented with true AIO rather than read/write/poll but using
 * poll on eventfd files to detect AIO events, rather than POSIX signals. */
int test_AIOE(struct vfi_dev *dev, struct vfi_source *src, struct gengetopt_args_info *opts)
{
	int i;
	int count = 0;
	char *cmd = NULL;
	int result = 0;
	void *ah;
	pthread_t tid[100];
	pthread_t clean;
	void *e;

	printf("Non-Blocking Out of Order Mode\n");

	pthread_create(&clean,0,thr_l,(void *)dev);

	while (vfi_get_cmd(src,&cmd)) {
		printf ("%s\n",cmd);
		ah = vfi_alloc_async_handle(NULL);
		result = vfi_invoke_cmd(dev, "%s?request(%p)\n", cmd, ah);
		pthread_create(&tid[count++],0,thr_f,(void *)ah);
	}

	for (i = 0; i < count; i++)
		pthread_join(tid[i],0);

	return result;
}

/* Wrapper to process the input source in one of the applciation
 * example styles... */
int process_commands(struct vfi_dev *dev, struct vfi_source *src, struct gengetopt_args_info *opts)
{
	int result;
	switch(opts->mode_arg) {
	case mode_arg_BO:
		result = test_BO(dev,src,opts);
		break;
	case mode_arg_NBIO:
		result = test_NBIO(dev,src,opts);
		break;
	case mode_arg_NBOO:
		result = test_NBOO(dev,src,opts);
		break;
	case mode_arg_AIOE:
		result = test_AIOE(dev,src,opts);
		break;
	}
	return result;
}

int get_inputs(void **s, char **command)
{
	struct gengetopt_args_info *opts = *s;
	*command = *opts->inputs++;
	return opts->inputs_num--;
}

int setup_inputs(struct vfi_dev *dev, struct vfi_source **src,struct gengetopt_args_info *opts)
{
	struct vfi_source *h = malloc(sizeof(*h)+sizeof(void *));
	*src = h;
	if (h == NULL)
		return -ENOMEM;
	h->f = get_inputs;
	h->d = (void *)dev;
	h->h[0] = (void *)opts;
	return 0;
}

int main (int argc, char **argv)
{
	struct gengetopt_args_info opts;
	struct vfi_source *s;
	struct vfi_dev *dev;

	vfi_open(&dev,NULL,opts.timeout_arg);

	cmdline_parser_init(&opts);

	cmdline_parser(argc,argv,&opts);

	if (opts.file_given) {
		vfi_setup_file(dev,&s,fopen(opts.file_arg,"r"));
		process_commands(dev,s,&opts);
	}

	if (opts.inputs_num) {
		setup_inputs(dev,&s,&opts);
		process_commands(dev,s,&opts);
	}

	if (opts.interactive_given) {
		vfi_setup_file(dev,&s,stdin);
		process_commands(dev,s,&opts);
	}

	vfi_close(dev);

	return 0;
}
