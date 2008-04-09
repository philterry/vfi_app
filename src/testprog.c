#include <vfi_api.h>
#include <semaphore.h>
#include <pthread.h>
#include <tp_cmdline.h>

/*
 * The following is iffy, pseudo-code to help thrash out the API design.
 */

/* A sample closure... */
void *do_pipe(void **e, char *result)
{
	char **imaps;
	int  **events;
	char **omaps;
	int numi;
	int nume;
	int numo;
	int i;

	int sig = (unsigned int)e[1] ^ (unsigned int)do_pipe;
	
	if (sig & ~0xffffff)
		return NULL;
	imaps = (char **)&e[2];
	numi = sig & 0xff;
	sig >>= 8;

	events = (int **)&e[2+numi];
	nume = sig & 0xff;
	sig >>= 8;

	omaps = (char **)&e[2+numi+nume];
	numo = sig & 0xff;

	for (i = 0; i < numi; i++)
		printf("imap[%d](%s)\n",i,imaps[i]);

	for (i = 0; i < nume; i++)
		printf("evnt[%d](%d)\n",i,events[i]);

	for (i = 0; i < numo; i++)
		printf("omap[%d](%s)\n",i,omaps[i]);

	return events[nume-1];
}

/* A sample internal command to create and deliver a closure... */
void **parse_pipe(struct vfi_dev *dev, struct vfi_async_handle *ah, char *cmd)
{
/* pipe://[<inmap><]*<func>[(<event>[,<event]*)][><omap>]*  */

	char *sp = cmd;
	int size = 0;
	int i = 0;
	int numpipe = 0;
	int outmaps = 0;
	int events = 0;
	int func = 0;
	int found_func = 0;
	int inmaps = 0;
	int numimaps = 0;
	int numomaps = 0;
	int numevnts = 0;
	void **pipe;
	char *elem[20];

	while (*sp) {
		if (sscanf(sp," %a[^<>,()]%n",&elem[i],&size) > 0) {
			switch (*(sp+size)) {
			case '<':
				inmaps = i;
				break;
			case '(':
				func = i;
				found_func = 1;
				break;
			case ')':
				events = i;
				break;
			case ',':
				break;
			case '>':
				if (!found_func) {
					func = i;
					found_func = 1;
				}
				outmaps = i;
				break;

			default:
				if (!found_func) {
					func = i;
					found_func = 1;
				}
				outmaps = i;
				break;
			}
			i++;
			sp += size;
		}
		else
			sp += 1;
	}
	numpipe = i + 1;
	numimaps = func;

	if (events)
		numevnts = events - func;
	else
		events = func;
	if (outmaps)
		numomaps = outmaps - events; 

	pipe = calloc(numpipe,sizeof(void *));
	pipe[0] = vfi_find_func(dev,elem[func]);
	free(elem[func]);

	for (i = 0; i< numimaps;i++) {
		pipe[i+2] = vfi_find_map(dev,elem[i]);
		free(elem[i]);
	}

	for (i = 0; i< numevnts;i++) {
		pipe[func+i+2] = vfi_find_event(dev,elem[func+i+1]);
		free(elem[func+i+1]);
	}

	for (i = 0; i< numomaps;i++) {
		pipe[events+i+2] = vfi_find_map(dev,elem[events+i+1]);
		free(elem[events+i+1]);
	}
	pipe[1] = (void *)(((numimaps & 0xff) << 0) | ((numevnts & 0xff) << 8) | ((numomaps & 0xff) << 16));

	pipe[1] =  (void *)((unsigned int)pipe[1] ^ (unsigned int)pipe[0]);
	return pipe;
}

/* An example of application processing style, in this case Blocking Ordered */
int test_BO(struct vfi_dev *dev, struct vfi_source *src, struct gengetopt_args_info *opts)
{
	char *output;
	char *cmd = NULL;
	int result = 1;
	void **e;
	void *done;

	printf("Blocking Ordered Mode\n");
	while (vfi_get_cmd(src,&cmd) && result) {

		/* Crap to debug closures... */
 		e = vfi_find_pre_cmd(dev,NULL,cmd);
 		if (e)
 			done = ((void *(*)(void **,char *))(e[0]))(e,output);
		
		/* ...end of crap */

		printf ("%s\n\t -> ",cmd);

		result = vfi_do_cmd(dev,&output, "%s\n", cmd);

		if (result < 0)
			break;

		result = vfi_get_dec_arg(output,"result");

		printf("%s\n",output);;

		free(output); 
	}

	return result;
}

/* Another example of application processing style, overlapped driver
 * command with processing but block at end of processing before next
 * driver invocation, ie Blocked Interleaved Ordered. */
int test_NBIO(struct vfi_dev *dev, struct vfi_source *src, struct gengetopt_args_info *opts)
{
	char *cmd = NULL;
	char *output;
	int result = 1;

	printf("Non-Blocking Interleaved Ordered Mode\n");

	while (vfi_get_cmd(src,&cmd) && result) {
		printf ("%s\n\t -> ",cmd);

		result = vfi_invoke_cmd(dev, "%s\n", cmd);
		if (result < 0)
			break;

		result = vfi_get_result(dev,&output);
		if (result < 0)
			break;

		result = vfi_get_dec_arg(output,"result");

		printf("%s\n",output);

		free(output);
	}

	return result;
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

	result = vfi_get_dec_arg(output,"result");

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
		e = vfi_find_pre_cmd(dev,ah,cmd);
		vfi_set_async_handle(ah,e);
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
		e = vfi_find_pre_cmd(dev,ah,cmd);
		vfi_set_async_handle(ah,e);
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

	char *im1 = vfi_register_map(dev,"im1",calloc(1,128));
	char *im2 = vfi_register_map(dev,"im2",calloc(1,128));
	char *om1 = vfi_register_map(dev,"om1",calloc(1,128));
	char *om2 = vfi_register_map(dev,"om2",calloc(1,128));

	strcpy(im1,"hello this is in map string one");
	strcpy(im2,"hello this is in map string two");
	strcpy(om1,"hello this is out map string one");
	strcpy(om2,"hello this is out map string two");

	vfi_register_func(dev,"show",do_pipe);
	vfi_register_event(dev,"e1",(void *)1);
	vfi_register_event(dev,"e2",(void *)2);

	vfi_register_pre_cmd(dev,"pipe",parse_pipe);

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
