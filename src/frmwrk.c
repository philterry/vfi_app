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

int setup_inputs(struct rddma_dev *dev, struct rddma_source **src, struct gengetopt_args_info *opts)
{
	struct rddma_source *h = malloc(sizeof(*h)+sizeof(void *[1]));
	*src = h;
	if (src == NULL)
		return -ENOMEM;

	h->f = get_inputs;
	h->d = dev;
	h->h[0] = (void *)opts;
	return 0;
}

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
void **parse_pipe(struct rddma_dev *dev, void *ah, char *cmd)
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
	pipe[0] = rddma_find_func(dev,elem[func]);
	free(elem[func]);

	for (i = 0; i< numimaps;i++) {
		pipe[i+2] = rddma_find_map(dev,elem[i]);
		free(elem[i]);
	}

	for (i = 0; i< numevnts;i++) {
		pipe[func+i+2] = rddma_find_event(dev,elem[func+i+1]);
		free(elem[func+i+1]);
	}

	for (i = 0; i< numomaps;i++) {
		pipe[events+i+2] = rddma_find_map(dev,elem[events+i+1]);
		free(elem[events+i+1]);
	}
	pipe[1] = (void *)(((numimaps & 0xff) << 0) | ((numevnts & 0xff) << 8) | ((numomaps & 0xff) << 16));

	pipe[1] =  (void *)((unsigned int)pipe[1] ^ (unsigned int)pipe[0]);
	return pipe;
}

void *source_thread(void *source)
{
	char *cmd = NULL;
	char *result = NULL;
	void **e;
	struct rddma_async_handle *ah;
	struct rddma_source *src = (struct rddma_source *)source;
	ah = rddma_alloc_async_handle(NULL);
	while (rddma_get_cmd(src,&cmd)) {
		rddma_set_async_handle(ah,NULL);
		rddma_find_pre_cmd(src->d, ah, cmd);
		rddma_invoke_cmd(src->d,"%s?request(%p)\n",cmd,ah);
		rddma_wait_async_handle(ah,&result,e);
		if (e)
			((void *(*)(void *))e[0])(e);
	}
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

int initialize_api_commands(struct rddma_dev *dev)
{
	char *im1 = rddma_register_map(dev,"im1",calloc(1,128));
	char *im2 = rddma_register_map(dev,"im2",calloc(1,128));
	char *om1 = rddma_register_map(dev,"om1",calloc(1,128));
	char *om2 = rddma_register_map(dev,"om2",calloc(1,128));

	strcpy(im1,"hello this is in map string one");
	strcpy(im2,"hello this is in map string two");
	strcpy(om1,"hello this is out map string one");
	strcpy(om2,"hello this is out map string two");

	rddma_register_func(dev,"show",do_pipe);
	rddma_register_event(dev,"e1",(void *)1);
	rddma_register_event(dev,"e2",(void *)2);

	rddma_register_pre_cmd(dev,"pipe",parse_pipe);
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
	
	initialize_api_commands(dev);

	pthread_create(&driver_tid,0,driver_thread,(void *)dev);

	while (opts.file_given--) {
		FILE *file = fopen(*opts.file_arg++,"r");
		if (!rddma_setup_file(dev,&src,file))
			ft = process_commands(&file_tid, src, &opts) == 0;
	}

	if (opts.inputs_num) {
		if (!setup_inputs(dev,&src,&opts))
			ot = process_commands(&opts_tid, src, &opts) == 0;
	}

	if (opts.interactive_given) {
		if (!rddma_setup_file(dev,&src,stdin))
			ut =process_commands(&user_tid, src, &opts) == 0;
	}

	if (ft)	pthread_join(file_tid,0);
	if (ot) pthread_join(opts_tid,0);
	if (ut) pthread_join(user_tid,0);

	done = 1;
	pthread_join(driver_tid,NULL);
	return 0;
}
