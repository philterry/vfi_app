#include <vfi_api.h>
#include <semaphore.h>
#include <pthread.h>
#include <fw_cmdline.h>
#include <sys/mman.h>

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
	if (src == NULL)
		return -ENOMEM;

	h->f = get_inputs;
	h->d = dev;
	h->h[0] = (void *)opts;
	return 0;
}

int bind_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* bind_create://x.xl.f/d.dl.f?event_name(dn)=s.sl.f?event_name(sn) */

	char *sl=NULL,*dl=NULL,*xl=NULL,*dst=NULL,*sen=NULL,*den=NULL;
	char *src;
	int size;
	
	sscanf(*cmd,   "%a[^/]%n",&xl,&size);
	src = *cmd + size;
	sscanf(*cmd+size,"%a[^=]%n",&dst,&size);
	src = src + size;
	
	vfi_get_str_arg(src,"event_name",&sen);
	vfi_get_str_arg(dst,"event_name",&den);

	vfi_get_location(src,&sl);
	vfi_get_location(dst,&dl);

	vfi_register_event(dev,sen,sl);
	vfi_register_event(dev,den,dl);

	free(xl);free(sl);free(dl);free(dst);free(sen);free(den);

	return 0;
}

int smb_mmap_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	long offset;
	void *mem;
	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_SHARED;

	struct vfi_map *p = e;
	if (!vfi_get_hex_arg(result,"mmap_offset",&offset)) {
		p->mem = mmap(0,p->extent,prot,flags,vfi_fileno(dev), offset);
		vfi_register_map(dev,p->name,e);
		vfi_set_async_handle(ah,NULL);
	}
	return 0;
}

int smb_mmap_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	char *name;
	if (vfi_get_str_arg(*cmd,"map_name",&name) > 0) {
		struct vfi_map *e;
		if (!vfi_alloc_map(&e,name)) {
			e->f = smb_mmap_closure;
			vfi_get_extent(*cmd,&e->extent);
			free(vfi_set_async_handle(ah,e));
		}
		free(name);
	}
	return 0;
}

int smb_create_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char **result)
{
	int i;
	char *smb;
	struct {void *f; char *name; char **cmd;} *p = e;
	struct vfi_map *me;
	vfi_alloc_map(&me,p->name);
	sscanf(*result+strlen("smb_create://"),"%a[^?]",&smb);
	sprintf(*p->cmd,"smb_mmap://%s?map_name(%s)\n",smb,p->name);
	free(smb);
	me->f = smb_mmap_closure;
	vfi_get_extent(*result,&me->extent);
	free(p->name);
	free(vfi_set_async_handle(ah,me));
	return 0;
}

int smb_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* smb_create://smb.loc.f#off:ext?map_name(name) */
	char *name;
	char *result = NULL;
	void **e;

	if (vfi_get_str_arg(*cmd,"map_name",&name) > 0) {
		struct {void *f; char *name; char **cmd;} *e = calloc(1,sizeof(*e));
		if (e) {
			e->f =smb_create_closure;
			e->name = name;
			e->cmd = cmd;
			free(vfi_set_async_handle(ah,e));
		}
	}
	return 0;
}

int event_find_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char **result)
{
	/* event_find://name.location */
	char *name;
	char *location;
	long err;
	int rc;

	rc = vfi_get_dec_arg(*result,"result",&err);
	if (!err && !rc) {
		rc = vfi_get_name_location(*result,&name,&location);
		if (!rc) {
			vfi_register_event(dev,name,location);
			free(name);free(location);
		}
	}
	return 0;
}

int event_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* event_find://name.location */
	free(vfi_set_async_handle(ah,event_find_closure));
	return 0;
}

int sync_find_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char **result)
{
	long rslt;
	int rc;
	rc = vfi_get_dec_arg(*result,"result",&rslt);
	if (rc)
		return 0;

	return rslt;
}

int sync_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* sync_find://name.location?wait */
	if (vfi_get_option(*cmd,"wait"))
		free(vfi_set_async_handle(ah,sync_find_closure));

	return 0;
}


/* A sample internal function of the sort which can be invoked by the
 * pipe API pre-command in conjunction with the source processing loop
 * thread. This function is polymorphic in that it parses its
 * signature to determine its input/output parameters. In reality real
 * functions would at most simply check their signature as a debug
 * reality check before processing their fixed function. */
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

/* The API pre-command to create and deliver a closure which executes
 * the named function on the named maps, invokes the chained events,
 * and is reinvoked on completion of the chained events. */
int pipe_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
/* pipe://[<inmap><]*<func>[(<event>[,<event>]*)][><omap>]*  */

	char *sp = *cmd;
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
	void *e;
	char *result = NULL;
	char *eloc;

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

	if (numevnts == 0) {
		while (i--)
			free(elem[i]);
		return 0;
	}

	pipe = calloc(numpipe-numevnts,sizeof(void *));
	vfi_find_func(dev,elem[func],&pipe[0]);
	free(elem[func]);

	for (i = 0; i< numimaps;i++) {
		vfi_find_map(dev,elem[i],(struct vfi_map **)&pipe[i+2]);
		free(elem[i]);
	}

	for (i = 0; i< numomaps;i++) {
		vfi_find_map(dev,elem[events+i+1],(struct vfi_map **)&pipe[func+i+2]);
		free(elem[func+i+1]);
	}

	pipe[1] = (void *)(((numimaps & 0xff) << 0) | ((numomaps & 0xff) << 8));
	pipe[1] =  (void *)((unsigned int)pipe[1] ^ (unsigned int)pipe[0]);

	if (numevnts > 1)
		for (i = 0; i< numevnts-1;i++) {
			vfi_find_event(dev,elem[func+i+1],(void **)&eloc);
			vfi_invoke_cmd(dev,"event_chain://%s.%s?request(%p),event_name(%s)\n",
				       elem[func+i+1],
				       eloc,
				       ah,
				       elem[func+1+2]);
			vfi_wait_async_handle(ah,&result,&e);
			if (i)
				free(elem[func+i+1]);
		}
	free(*cmd);
	*cmd = malloc(128);
	vfi_find_event(dev,elem[func+1],(void **)&eloc);
	i = snprintf(*cmd,128,"event_start://%s.%s?request(%p)\n",elem[func+1],eloc,ah);
	if (i >= 128) {
		*cmd = realloc(*cmd,i+1);
		snprintf(*cmd,128,"event_start://%s.%s\n",elem[func+1],eloc);
	}

	free(elem[func+1]);
	free(vfi_set_async_handle(ah,pipe));

	return 1;
}

int quit_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* quit or quit:// */
	vfi_set_dev_done(dev);
	return 0;
}

int map_init_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* map_init://name#o:e?init_val(x) */
	char *name;
	char *location;
	long long offset;
	long extent;
	long val;
	struct vfi_map *map;
	long *mem;
	int rc;

	rc = vfi_get_name_location(*cmd, &name, &location);
	free(location);
	if (rc)
		goto out;
	if (vfi_get_offset(*cmd,&offset))
		goto out;
	if (vfi_get_extent(*cmd,&extent))
		goto out;
	if (vfi_get_hex_arg(*cmd,"init_val",&val))
		goto out;
	if (vfi_find_map(dev,name,&map))
		goto out;
	if (map->extent < offset + extent)
		goto out;

	free(name);

	mem = (long *)map->mem + offset;
	extent = extent >> 2;

	while  (extent--)
		*mem++ = val;

	return 1;
out:
	free(name);
	return 0;
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
		free(vfi_set_async_handle(ah,NULL));
		if (!vfi_find_pre_cmd(src->d, ah, &cmd)) {
			do {
				vfi_invoke_cmd(src->d,"%s%srequest(%p)\n",cmd,strstr(cmd, "?") ? ",":"?",ah);
				vfi_wait_async_handle(ah,&result,(void *)&e);
				printf("%s\n",result);
			} while (vfi_invoke_closure(e,src->d,ah,result));
		}
	}

	free(result);
	free(e);

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
	struct vfi_source *src;

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
		if (!vfi_setup_file(dev,&src,file))
			ft = process_commands(&file_tid, src, &opts) == 0;
	}

	if (opts.inputs_num) {
		if (!setup_inputs(dev,&src,&opts))
			ot = process_commands(&opts_tid, src, &opts) == 0;
	}

	if (opts.interactive_given) {
		if (!vfi_setup_file(dev,&src,stdin))
			ut = process_commands(&user_tid, src, &opts) == 0;
	}

	if (ft) pthread_join(file_tid,0);
	if (ot) pthread_join(opts_tid,0);
	if (ut) pthread_join(user_tid,0);

	vfi_set_dev_done(dev);

	pthread_join(driver_tid,NULL);
	vfi_close(dev);

	return 0;
}
