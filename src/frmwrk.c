#include <vfi_api.h>
#include <pthread.h>
#include <fw_cmdline.h>
#include <vfi_frmwrk.h>

int get_inputs(void **s, char **command)
{
	struct gengetopt_args_info *opts = *s;
	*command = *(opts->inputs++);
	return opts->inputs_num--;
}

int setup_inputs(struct vfi_dev *dev, struct vfi_source **src, struct gengetopt_args_info *opts)
{
	struct vfi_source *h = malloc(sizeof(*h)+sizeof(void *));
	*src = h;
	if (*src == NULL)
		return -ENOMEM;

	h->f = get_inputs;
	h->d = dev;
	h->h[0] = (void *)opts;
	return 0;
}


static const int max_count = 100000;
static int count = 0;

int count_function(void **e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	int bs = 0;
	int i;
	int sig = (unsigned int)e[1] ^ (unsigned int)count_function;
	
	if (sig & ~0xffff)
		return 0;

	printf("Transfer Count = %d%n", ++count, &bs);

	if (count == max_count) {
		printf("\n");
		count = 0;
		return 0;
	}

	for (; bs > 0; bs--)
		printf("\b");

	return 1;
}

int show_function(void **e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	struct vfi_map **imaps;
	struct vfi_map **omaps;
	int numi;
	int numo;
	int i;

	int sig = (unsigned int)e[1] ^ (unsigned int)show_function;
	
	if (sig & ~0xffff)
		return 0;

	imaps = (struct vfi_map **)&e[2];
	numi = sig & 0xff;

	sig >>= 8;

	omaps = (struct vfi_map **)&e[2+numi];
	numo = sig & 0xff;

	/* printf("%s: numi(%d), nume(%d), numo(%d)\n", __func__, numi, nume, numo); */

	for (i = 0; i < numi; i++)
		printf("imap[%d](%s)%d\n",i,imaps[i]->name,imaps[i]->extent);

	for (i = 0; i < numo; i++)
		printf("omap[%d](%s)%d\n",i,omaps[i]->name,omaps[i]->extent);

	return 1;
}

int copy_function(void **e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	struct vfi_map **imaps;
	struct vfi_map **omaps;
	int numi;
	int numo;
	int in = 0,out = 0;
	int done = 0;
	int size;

	int sig = (unsigned int)e[1] ^ (unsigned int)show_function;
	
	if (sig & ~0xffff)
		return 0;

	imaps = (struct vfi_map **)&e[2];
	numi = sig & 0xff;

	sig >>= 8;

	omaps = (struct vfi_map **)&e[2+numi];
	numo = sig & 0xff;

	while (!done) {
		size = (omaps[out]->extent < imaps[in]->extent) ? omaps[out]->extent : imaps[in]->extent;
		memcpy(omaps[out]->mem,imaps[in]->mem,size);

		if (out + 1 == numo && in + 1 == numi)
			done = 1;

		if (out + 1 < numo)
			out++;

		if (in + 1 < numi)
			in++;
	}
	return 1;
}

void *source_thread(void *source)
{
	char *cmd = NULL;
	char *result = NULL;
	void **e;
	struct vfi_async_handle *ah;
	struct vfi_source *src = (struct vfi_source *)source;
	ah = vfi_alloc_async_handle(NULL);
	while (vfi_get_cmd(src,&cmd)) {
		if (!vfi_find_pre_cmd(src->d, ah, &cmd)) {
			do {
				vfi_invoke_cmd(src->d,"%s%srequest(%p)\n",cmd,strstr(cmd, "?") ? ",":"?",ah);
				vfi_wait_async_handle(ah,&result,(void *)&e);
				//printf("%s\n",result);
			} while (vfi_invoke_closure(e,src->d,ah,result));
		}
		free(vfi_set_async_handle(ah,NULL));
	}

	free(result);
	vfi_free_async_handle(ah);

	return 0;
}

int process_commands(pthread_t *tid, struct vfi_source *src, struct gengetopt_args_info *opts)
{
	return pthread_create(tid,0,source_thread,(void *)src);
}

void *driver_thread(void *h)
{
	struct vfi_dev *dev = (struct vfi_dev *)h;
	while (!vfi_dev_done(dev))
		vfi_post_async_handle(dev);
	
	return 0;
}

int initialize_api_commands(struct vfi_dev *dev)
{
	vfi_register_func(dev,"show",show_function);
	vfi_register_func(dev,"copy",copy_function);
	vfi_register_func(dev,"count",count_function);

	vfi_register_pre_cmd(dev,"pipe",pipe_pre_cmd);
	vfi_register_pre_cmd(dev,"bind_create",bind_create_pre_cmd);
	vfi_register_pre_cmd(dev,"smb_create",smb_create_pre_cmd);
	vfi_register_pre_cmd(dev,"smb_mmap",smb_mmap_pre_cmd);
	vfi_register_pre_cmd(dev,"sync_find",sync_find_pre_cmd);
	vfi_register_pre_cmd(dev,"event_find",event_find_pre_cmd);
	vfi_register_pre_cmd(dev,"quit",quit_pre_cmd);

	return 0;
}

int main (int argc, char **argv)
{
	struct gengetopt_args_info opts;
	struct vfi_dev *dev;
	struct vfi_source *file_src = NULL;
	struct vfi_source *stdin_src = NULL;
	struct vfi_source *input_src = NULL;

	pthread_t driver_tid;

	pthread_t file_tid;
	pthread_t opts_tid;
	pthread_t user_tid;

	int ft = 0;
	int ot = 0;
	int ut = 0;

	int rc;

	cmdline_parser_init(&opts);
	cmdline_parser(argc,argv,&opts);

	rc = vfi_open(&dev,opts.device_arg,opts.timeout_arg);
	if (rc)
		return rc;
	
	initialize_api_commands(dev);

	pthread_create(&driver_tid,0,driver_thread,(void *)dev);
	
	while (opts.file_given--) {
		FILE *file = fopen(*opts.file_arg++,"r");
		if (!vfi_setup_file(dev,&file_src,file))
			ft = process_commands(&file_tid, file_src, &opts) == 0;
	}

	if (opts.inputs_num) {
		if (!setup_inputs(dev,&input_src,&opts))
			ot = process_commands(&opts_tid, input_src, &opts) == 0;
	}

	if (opts.interactive_given) {
		if (!vfi_setup_file(dev,&stdin_src,stdin))
			ut = process_commands(&user_tid, stdin_src, &opts) == 0;
	}

	if (ft) pthread_join(file_tid,0);
	if (ot) pthread_join(opts_tid,0);
	if (ut) pthread_join(user_tid,0);

	vfi_set_dev_done(dev);
	pthread_join(driver_tid,NULL);

	if (file_src)
		vfi_teardown_file(file_src);
	if (stdin_src)
		vfi_teardown_file(stdin_src);
	if (input_src)
		free(input_src);

	vfi_close(dev);

	// We're messing with the opts so don't free it.
	//cmdline_parser_free(&opts);

	return 0;
}
