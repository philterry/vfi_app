#include <rddma_api.h>
#include <semaphore.h>
#include <pthread.h>
#include <tp_cmdline.h>

struct cmd_elem {
	struct cmd_elem *next;
	int (*f)(struct rddma_dev *, char *);
	int size;
	char *cmd;
	char b[];
};

int dummy_cmd(struct rddma_dev *dev, char *cmd)
{
	printf("%s:%s\n",__FUNCTION__,cmd);
	return 1;
}

struct cmd_elem commands[] = {
	{0,dummy_cmd,0,"map"},
	{0,dummy_cmd,0,"pipe"},
	{0,0,0,""},
};

int init_cmd_elem_ary(struct cmd_elem *ary)
{
	int i;
	for (i = 0; ary[i].cmd[0] ; i++) {
		ary[i].next = &ary[i+1];
		ary[i].size = strlen(&ary[i].cmd[0]);
	}
}

int internal_cmd(struct rddma_dev *dev, char *buf, int sz)
{
	struct cmd_elem *cmd;
	char *term;
	term = strstr(buf,"://");
	
	if (term) {
		int size = term - buf;
		
		for (cmd = &commands[0];cmd && cmd->f; cmd = cmd->next)
			if (size == cmd->size && !strncmp(buf,cmd->cmd,size))
				return cmd->f(dev,term+3);
	}
	return 0;
}

int register_cmd(char *name, int (*f)(struct rddma_dev *,char *))
{
	struct cmd_elem *c;
	int len = strlen(name);
	c = calloc(1,sizeof(*c)+len+1);
	strcpy(c->b,name);
	c->cmd = c->b;
	c->size = len;
	c->f = f;
	c->next = commands[0].next;
	commands[0].next = c;
}

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
	int ret;
	FILE *fp = (FILE *)s;
	ret = fscanf(fp,"%a[^\n]",command);
	if (ret > 0)
		fgetc(fp);
	return ret > 0;
}

void *setup_file(FILE *fp)
{
	struct source_handle *h = malloc(sizeof(*h));
	h->f = get_file;
	h->h = (void *)fp;
	return (void *)h;
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

		result = rddma_get_dec_arg(output,"result");

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

		result = rddma_get_dec_arg(output,"result");

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
	
	result = rddma_get_dec_arg(output,"result");

	printf("\t -> %s\n",output);
	fflush(stdout);

	free(output); 
	rddma_free_async_handle(h);
	pthread_exit(0);
}

void *thr_l(void *h)
{
	struct rddma_dev *dev = (struct rddma_dev *)h;
	while (1)
		rddma_get_result_async(dev);
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
	pthread_t clean;

	printf("Non-Blocking Out of Order Mode\n"); 
	dev = rddma_open(NULL,opts->timeout_arg);

	pthread_create(&clean,0,thr_l,(void *)dev);

	while ( rddma_get_cmd(dev,h,&cmd,&size)) {

		printf ("%s\n",cmd);
		ah = rddma_alloc_async_handle();
		result = rddma_invoke_cmd(dev, "%s?request(%p)\n", cmd,ah);
		pthread_create(&tid[count++],0,thr_f,(void *)ah);
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

	init_cmd_elem_ary(&commands[0]);
	register_cmd("fred",dummy_cmd);

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
