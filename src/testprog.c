#include <rddma_api.h>
#include <semaphore.h>
#include <pthread.h>
#include <tp_cmdline.h>


/*
 * The following is iffy, pseudo-code to help thrash out the API design.
 *
 * The command and the element stuff may get coalesced into a single
 * concept, basically both are partial implementations of closures so
 * I may just go the whole hog and implement that wholesale.
 *
 */
struct cmd_elem {
	struct cmd_elem *next;
	void **(*f)(struct rddma_dev *, char *);
	int size;
	char *cmd;		/* this is the name of the command is self->b */
	char b[];		/* Holds the name of the card, pointed
				 * to by cmd above. */
};

void *dummy_cmd(struct rddma_dev *dev, char *cmd)
{
	printf("%s:%s\n",__FUNCTION__,cmd);
	return (void *)1;
}

/* These globals will eventually end up inside dev, or somewhere... */
struct cmd_elem *pre_commands;
struct cmd_elem *post_commands;

/* Find by name and execute an internal command. If we make internal
 * commands closures then we can do more than just pass them the
 * command string... */
void *internal_cmd(struct rddma_dev *dev, char *buf, int sz)
{
	struct cmd_elem *cmd;
	char *term;
	term = strstr(buf,"://");
	
	if (term) {
		int size = term - buf;
		
		for (cmd = pre_commands;cmd && cmd->f; cmd = cmd->next)
			if (size == cmd->size && !strncmp(buf,cmd->cmd,size))
				return cmd->f(dev,term+3);
	}
	return 0;
}

/* 
 * Register commands to the global lists... should eventually pass dev
 * or something... */
int register_pre_cmd(char *name, void **(*f)(struct rddma_dev *,char *))
{
	struct cmd_elem *c;
	int len = strlen(name);
	c = calloc(1,sizeof(*c)+len+1);
	strcpy(c->b,name);
	c->cmd = c->b;
	c->size = len;
	c->f = f;
	c->next = pre_commands;
	pre_commands = c;
}

int register_post_cmd(char *name, void **(*f)(struct rddma_dev *,char *))
{
	struct cmd_elem *c;
	int len = strlen(name);
	c = calloc(1,sizeof(*c)+len+1);
	strcpy(c->b,name);
	c->cmd = c->b;
	c->size = len;
	c->f = f;
	c->next = post_commands;
	post_commands = c;
}

/* Implement closures */
struct look_elem {
	struct look_elem *next;
	char *name;		/* name of closure self->b */
	int size;		/* size of name */
	void *e;		/* closure */
	char b[];		/* buffer for name */
};

/* Global lists should go somewhere, dev? */
struct look_elem *maps;
struct look_elem *events;
struct look_elem *funcs;

/* lookup named closures in global lists. */
void *lookup_elem(struct look_elem *elems,char *name)
{
	struct look_elem *l;
	int size = strlen(name);

	for (l = elems; l; l = l->next) {
		if ((size == l->size) && !strcmp(l->name,name)) {
			return l->e;
		}
	}
	return NULL;
}

void *lookup_func(char *name)
{
	void *ret;
	ret = lookup_elem(funcs,name);
	return ret;
}

void *lookup_map(char *name)
{
	void * ret;
	ret = lookup_elem(maps,name);
	return ret;
}

void *lookup_event(char *name)
{
	void *ret;
	ret = lookup_elem(events,name);
	return ret;
}

/* Register closures in global lists, eventually dev? */
void *register_elem(struct look_elem **elems, char *name, void *e)
{
	struct look_elem *l;
	if (lookup_elem(*elems,name))
		return 0;
	l = calloc(1,sizeof(*l)+strlen(name)+1);
	l->size = strlen(name);
	strcpy(l->b,name);
	l->name = l->b;
	l->e = e;
	l->next = *elems;
	*elems = l;
	return e;
}

void *register_map(char *name, void *e)
{
	return register_elem(&maps,name,e);
}

void *register_func(char *name, void *e)
{
	return register_elem(&funcs,name,e);
}

void *register_event(char *name, void *e)
{
	return register_elem(&events,name,e);
}


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
void **parse_pipe(struct rddma_dev *dev, char *cmd)
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
	pipe[0] = lookup_func(elem[func]);
	free(elem[func]);

	for (i = 0; i< numimaps;i++) {
		pipe[i+2] = lookup_map(elem[i]);
		free(elem[i]);
	}

	for (i = 0; i< numevnts;i++) {
		pipe[func+i+2] = lookup_event(elem[func+i+1]);
		free(elem[func+i+1]);
	}

	for (i = 0; i< numomaps;i++) {
		pipe[events+i+2] = lookup_map(elem[events+i+1]);
		free(elem[events+i+1]);
	}
	pipe[1] = (void *)(((numimaps & 0xff) << 0) | ((numevnts & 0xff) << 8) | ((numomaps & 0xff) << 16));

	pipe[1] =  (void *)((unsigned int)pipe[1] ^ (unsigned int)pipe[0]);
	return pipe;
}

/*
 * Generalize input of command strings from multiple sources, command
 * line parameters (inputs), stream read (from files or stdin).
 */
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

	if (*command)
		free(*command);

	return (src->f(src->h,command,size) > 0);
}

/* An example of application processing style, in this case Blocking Ordered */
int test_BO(void *h, struct gengetopt_args_info *opts)
{
	int i;
	struct rddma_dev *dev;
	char *output;
	char *cmd = NULL;
	int size = 0;
	int result = 1;
	void **e;
	void *done;

	printf("Blocking Ordered Mode\n");
	dev = rddma_open(NULL,opts->timeout_arg);
	while (rddma_get_cmd(dev,h,&cmd,&size) && result) {

		/* Crap to debug closures... */
 		e = internal_cmd(dev,cmd,size);
 		if (e)
 			done = ((void *(*)(void **,char *))(e[0]))(e,output);
		
		/* ...end of crap */

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

/* Another example of application processing style, overlapped driver
 * command with processing but block at end of processing before next
 * driver invocation, ie Blocked Interleaved Ordered. */
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

/* These are just example threads... */
void *thr_f(void *h)
{
	char *output;
	int result;
	void **e;
	int done = 0;

	while (!done) {
		result = rddma_get_async_handle(h,&output,(void **)&e);
		if (e)
			done = ((int (*)(void **,char *))(e[0]))(e,output);
		else
			done = 1;
	}

	result = rddma_get_dec_arg(output,"result");

	printf("\t -> %s\n",output);
	fflush(stdout);

	free(output); 
	rddma_free_async_handle(h);
	pthread_exit(0);
}

/* This is the main dispatcher loop thread started by main before
 * spawning dependent threads above... */
void *thr_l(void *h)
{
	struct rddma_dev *dev = (struct rddma_dev *)h;
	while (1)
		rddma_get_result_async(dev);
}

/* Another example of application processing style, completely out of
 * order overlapped processing with driver invocations using
 * applicaton threads. */
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
	void **e = NULL;

	printf("Non-Blocking Out of Order Mode\n"); 
	dev = rddma_open(NULL,opts->timeout_arg);

	pthread_create(&clean,0,thr_l,(void *)dev);

	while ( rddma_get_cmd(dev,h,&cmd,&size)) {
		printf ("%s\n",cmd);
		e = internal_cmd(dev,cmd,size);
		ah = rddma_alloc_async_handle(e);
		result = rddma_invoke_cmd(dev, "%s?request(%p)\n", cmd,ah);
		pthread_create(&tid[count++],0,thr_f,(void *)ah);
	}

	for (i = 0; i < count; i++)
		pthread_join(tid[i],0);

	rddma_close(dev);

	return result;
}

/* Another application processing style example, same as NBOO but
 * implemented with true AIO rather than read/write/poll but using
 * poll on eventfd files to detect AIO events, rather than POSIX signals. */
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
	pthread_t clean;
	void *e;

	printf("Non-Blocking Out of Order Mode\n");
	dev = rddma_open(NULL,opts->timeout_arg);

	pthread_create(&clean,0,thr_l,(void *)dev);

	while (rddma_get_cmd(dev,h,&cmd,&size)) {
		printf ("%s\n",cmd);
		ah = rddma_alloc_async_handle(e);
		result = rddma_invoke_cmd(dev, "%s?request(%p)\n", cmd, ah);
		pthread_create(&tid[count++],0,thr_f,(void *)ah);
	}

	for (i = 0; i < count; i++)
		pthread_join(tid[i],0);

	rddma_close(dev);
	
	return result;
}

/* Wrapper to process the input source in one of the applciation
 * example styles... */
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

	char *im1 = register_map("im1",calloc(1,128));
	char *im2 = register_map("im2",calloc(1,128));
	char *om1 = register_map("om1",calloc(1,128));
	char *om2 = register_map("om2",calloc(1,128));

	strcpy(im1,"hello this is in map string one");
	strcpy(im2,"hello this is in map string two");
	strcpy(om1,"hello this is out map string one");
	strcpy(om2,"hello this is out map string two");

	register_func("show",do_pipe);
	register_event("e1",(void *)1);
	register_event("e2",(void *)2);

	register_pre_cmd("pipe",parse_pipe);

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
