#include <vfi_api.h>
#include <vfi_frmwrk.h>
#include <vfi_log.h>
#include <pthread.h>
#include <time.h>
#include <fw_cmdline.h>
#include <mcheck.h>
#include <stdbool.h>

#define MY_ERROR VFI_DBG_DEFAULT
#define MY_DEBUG (VFI_DBG_EVERYONE | VFI_DBG_EVERYTHING | VFI_LOG_DEBUG)

struct thread_arg 
{
	struct vfi_source *src;
	bool verbose;
	bool stop_on_error;
};

#define MAX_COUNT 10000
static int count = 0;

int get_inputs(void **s, char **command)
{
	struct {char **inputs; int inputs_num;} *input = *s;
	*command = *(input->inputs++);
	return input->inputs_num--;
}

int setup_inputs(struct vfi_dev *dev, struct vfi_source **src, char **inputs, int inputs_num)
{
	struct vfi_source *h = malloc(sizeof(*h)+sizeof(void *));
	struct {char **inputs; int inputs_num;} *inp = calloc(1,sizeof(*inp));
	inp->inputs = inputs;
	inp->inputs_num = inputs_num;
	*src = h;
	if (*src == NULL)
		return -ENOMEM;

	h->f = get_inputs;
	h->d = dev;
	h->h[0] = (void *)inp;
	return 0;
}

int count_function(void **e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	long rslt;
	int sig = (unsigned int)e[1] ^ (unsigned int)count_function;
	
	if (sig & ~0xffff) {
		vfi_log(VFI_LOG_ERR, "%s: Closure has errors. Stopping", __func__);
		return VFI_RESULT(-EINVAL);
	}

	if (vfi_get_dec_arg(result,"result",&rslt)) {
		vfi_log(VFI_LOG_EMERG, "%s: Fatal error. Result string not returned from driver", __func__);
		return VFI_RESULT(-EIO);
	}

	if (rslt) {
		vfi_log(VFI_LOG_ERR, "%s: Command failed with error %ld (%s)", __func__, rslt, result);
		return VFI_RESULT((int)rslt);
	}

	printf("Transfer Count = %d", ++count);

	if (count == MAX_COUNT) {
		printf("\n");
		count = 0;
		return 0;
	}

	printf("\r");
	return 1;
}

int send_function(void **e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	long rslt;
	int sig = (unsigned int)e[1] ^ (unsigned int)count_function;
	
	if (sig & ~0xffff) {
		vfi_log(VFI_LOG_ERR, "%s: Closure has errors. Stopping", __func__);
		return VFI_RESULT(-EINVAL);
	}

	if (vfi_get_dec_arg(result,"result",&rslt)) {
		vfi_log(VFI_LOG_EMERG, "%s: Fatal error. Result string not returned from driver", __func__);
		return VFI_RESULT(-EIO);
	}

	if (rslt) {
		vfi_log(VFI_LOG_ERR, "%s: Command failed with error %ld (%s)", __func__, rslt, result);
		return VFI_RESULT((int)rslt);
	}

	if (++count == MAX_COUNT) {
		printf("Send done. Did %d transfers\n", count);
		count = 0;
		return 0;
	}

	return 1;
}

static void print_perf(struct timespec *start, struct timespec *stop, double nbytes, double incr)
{
    double            secs;
    char              bytes_str[64];
    double            bytes;
    char              speed_str[64];
    double            bytes_per_sec;
    char              incr_str[64];
    struct timespec   time;

    time.tv_sec = stop->tv_sec - start->tv_sec;
    time.tv_nsec = stop->tv_nsec - start->tv_nsec;

    secs = ((double) time.tv_sec) +
           ( (double)time.tv_nsec / (double)1000000000 );

    /* Make the incr size more readable */
    sprintf(incr_str, "B");
    if (incr >= (double)1024*1024*1024) {
	    sprintf(incr_str, "GiB");
	    incr /= (double)1024 * 1024 * 1024;
    }
    else if (incr >= (double)1024 * 1024) {
	    sprintf(incr_str, "MiB");
	    incr /= (double)1024 * 1024;
    }
    else if (incr >= (double)1024) {
	    sprintf(incr_str, "KiB");
	    incr /= (double)1024;
    }

    /* Make the number of bytes more readable */
    sprintf(bytes_str, "B");
    bytes = nbytes;
    if (nbytes >= (double)1000000000) {
	    sprintf(bytes_str, "GB");
	    bytes = nbytes/(double)1000000000;
    }
    else if (nbytes >= (double)1000000) {
	    sprintf(bytes_str, "MB");
	    bytes = nbytes/(double)1000000;
    }
    else if (nbytes >= (double)1000) {
	    sprintf(bytes_str, "KB");
	    bytes = nbytes/(double)1000;
    }

    /* Make the performance more readable */
    sprintf(speed_str, "B/s");
    bytes_per_sec = nbytes/secs;
    if (nbytes/secs >= (double)1000000) {
        sprintf(speed_str, "MB/s");
        bytes_per_sec = nbytes/secs/(double)1000000;
    }
    else if (nbytes/secs >= (double)1000) {
        sprintf(speed_str, "KB/s");
        bytes_per_sec = nbytes/secs/(double)1000;
    }

    printf ("\n"
            "----------------------------------\n"
            "  Last increment : %2.2f %s\n"
            "  Bytes in total : %2.2f %s\n"
            "  Seconds used   : %2.2f\n"
            "  Transfer rate  : %2.3f %s\n"
            "----------------------------------\n",
            incr, incr_str,
            bytes, bytes_str,
            secs,
            bytes_per_sec, speed_str );
}

int perf_function(void **e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	long rslt;
	static struct timespec start;
	static struct timespec stop;
	static double nbytes;
	int sig = (unsigned int)e[1] ^ (unsigned int)count_function;
	struct vfi_map *map = (struct vfi_map *)e[2];
	
	if (sig & ~0xffff) {
		vfi_log(VFI_LOG_ERR, "%s: Closure has errors. Stopping", __func__);
		return VFI_RESULT(-EINVAL);
	}
	
	if (vfi_get_dec_arg(result,"result",&rslt)) {
		vfi_log(VFI_LOG_EMERG, "%s: Fatal error. Result string not returned from driver", __func__);
		return VFI_RESULT(-EIO);
	}

	if (rslt) {
		vfi_log(VFI_LOG_ERR, "%s: Command failed with error %ld (%s)", __func__, rslt, result);
		return VFI_RESULT((int)rslt);
	}

	switch (++count) {

	case 1:
		break;

	case 2:
		if (clock_gettime(CLOCK_REALTIME, &start)) {
			vfi_log(VFI_LOG_ERR, "%s: clock_gettime failed with error %d", __func__, errno);
			return VFI_RESULT(-errno);
		}
		nbytes = 0;
		break;

	case MAX_COUNT:
		if (clock_gettime(CLOCK_REALTIME, &stop)) {
			vfi_log(VFI_LOG_ERR, "%s: clock_gettime failed with error %d", __func__, errno);
			return VFI_RESULT(-errno);
		}
		nbytes += (double)map->extent; 
		print_perf(&start, &stop, nbytes, (double)map->extent);
		count = 0;
		return 0;

	default:
		nbytes += map->extent; 
		break;
	}

	return 1;
}

int show_function(void **e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	struct vfi_map **imaps;
	struct vfi_map **omaps;
	int numi;
	int numo;
	int i;
	long rslt;

	int sig = (unsigned int)e[1] ^ (unsigned int)show_function;
	
	if (sig & ~0xffff) {
		vfi_log(VFI_LOG_ERR, "%s: Closure has errors. Stopping", __func__);
		return VFI_RESULT(-EINVAL);
	}

	if (vfi_get_dec_arg(result,"result",&rslt)) {
		vfi_log(VFI_LOG_EMERG, "%s: Fatal error. Result string not returned from driver", __func__);
		return VFI_RESULT(-EIO);
	}

	if (rslt) {
		vfi_log(VFI_LOG_ERR, "%s: Command failed with error %ld (%s)", __func__, rslt, result);
		return VFI_RESULT((int)rslt);
	}


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
	long rslt;

	int sig = (unsigned int)e[1] ^ (unsigned int)show_function;
	
	if (sig & ~0xffff) {
		vfi_log(VFI_LOG_ERR, "%s: Closure has errors. Stopping", __func__);
		return VFI_RESULT(-EINVAL);
	}

	if (vfi_get_dec_arg(result,"result",&rslt)) {
		vfi_log(VFI_LOG_EMERG, "%s: Fatal error. Result string not returned from driver", __func__);
		return VFI_RESULT(-EIO);
	}

	if (rslt) {
		vfi_log(VFI_LOG_ERR, "%s: Command failed with error %ld (%s)", __func__, rslt, result);
		return VFI_RESULT((int)rslt);
	}

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

void *source_thread(void *arg)
{
	char *cmd = NULL;
	char *result = NULL;
	void **e;
	int err = 0;
	long rslt = 0;
	struct vfi_async_handle *ah;
	struct thread_arg *targ = (struct thread_arg *)arg;
	struct vfi_source *src = (struct vfi_source *)targ->src;
	ah = vfi_alloc_async_handle(NULL);
	if (!ah) {
		vfi_log(VFI_LOG_ERR, "%s: Failed to allocate async handle", __func__);
		return 0;
	}

	while (vfi_get_cmd(src,&cmd)) {
		VFI_DEBUG(MY_DEBUG, "%s Got command: %s\n", __func__, cmd);
		if (!(err = vfi_find_pre_cmd(src->d, ah, &cmd))) {
			if (targ->verbose)
				printf("%s\n",cmd);
			do {
				vfi_invoke_cmd(src->d,"%s%srequest(%p)\n",cmd,strstr(cmd, "?") ? ",":"?",ah);
				vfi_wait_async_handle(ah,&result,(void *)&e);
#if 0
#warning TODO: Remove. 
				if (vfi_get_dec_arg(result, "result", &rslt))
					printf("Fatal. Result not found: %s\n", result);
				else if (rslt)
					printf("Cmd failed: %s\n", result);
#endif
			} while ((err = (int)vfi_invoke_closure(e,src->d,ah,result)) > 0);

			if (targ->verbose)
				printf("%s\n",result);
		}

		free(vfi_set_async_handle(ah,NULL));
		if (err < 0) {
			vfi_log(VFI_LOG_ERR, "%s: Command processing resulted in error %d\n", __func__, err);
			if (targ->stop_on_error)
				break;
		}
	}

	free(result);
	vfi_free_async_handle(ah);
	return 0;
}

int process_commands(pthread_t *tid, struct thread_arg *targ, bool spawn)
{
	if (spawn)
		return pthread_create(tid,0,source_thread,(void *)targ);
	else
		source_thread((void *)targ);
	return 0;
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
	vfi_register_func(dev,"show",show_function,-1,-1);
	vfi_register_func(dev,"copy",copy_function,-1,-1);
	vfi_register_func(dev,"count",count_function,1,1);
	vfi_register_func(dev,"send",send_function,-1,-1);
	vfi_register_func(dev,"perf",perf_function,-1,-1);

	vfi_register_pre_cmd(dev,"location_find",location_find_pre_cmd);
	vfi_register_pre_cmd(dev,"pipe",pipe_pre_cmd);
	vfi_register_pre_cmd(dev,"bind_create",bind_create_pre_cmd);
	vfi_register_pre_cmd(dev,"smb_create",smb_create_pre_cmd);
	vfi_register_pre_cmd(dev,"smb_mmap",smb_mmap_pre_cmd);
	vfi_register_pre_cmd(dev,"sync_find",sync_find_pre_cmd);
	vfi_register_pre_cmd(dev,"event_find",event_find_pre_cmd);

	vfi_register_pre_cmd(dev,"map_init",map_init_pre_cmd);
	vfi_register_pre_cmd(dev,"map_check",map_check_pre_cmd);

	vfi_register_pre_cmd(dev,"quit",quit_pre_cmd);

	return 0;
}

int main (int argc, char **argv)
{
	struct gengetopt_args_info opts;
	struct vfi_dev *dev;
	pthread_t driver_tid;
	int rc;
	int cnt;
	
	struct thread_info 
	{
		pthread_t tid;
		struct thread_arg targ;
		struct thread_info *next;
		bool do_teardown;
	};
		
	struct thread_info *tinfo_list = NULL;


#ifdef VFI_DBG
	vfi_debug_level = MY_DEBUG;
#endif

	/* Simple memory alloc and free tracing */
	mtrace();

	cmdline_parser_init(&opts);
	cmdline_parser(argc,argv,&opts);

	rc = vfi_open(&dev,opts.device_arg,opts.timeout_arg);
	if (rc)
		return rc;
	
	initialize_api_commands(dev);

	pthread_create(&driver_tid,0,driver_thread,(void *)dev);
	
	for (cnt = 0; cnt < opts.file_given; cnt++) {
		FILE *file;
		struct thread_info *tinfo = calloc (1,sizeof(struct thread_info));
		if (tinfo == NULL)
			break;

		file = fopen(opts.file_arg[cnt],"r");
		if (!file) {
			vfi_log(VFI_LOG_ERR, "%s: Failed to open file %s", __func__, opts.file_arg[cnt]);
			free(tinfo);
			break;
		}

		tinfo->targ.stop_on_error = true;
	
		if (vfi_setup_file(dev,&(tinfo->targ.src),file)) {
			free(tinfo);
			continue;
		}

		if (process_commands(&(tinfo->tid),&(tinfo->targ),true)) {
			free(tinfo);
			break;
		}
		
		tinfo->next = tinfo_list;
		tinfo_list = tinfo;
	}

	if (opts.inputs_num) {
		struct thread_info *tinfo = calloc (1,sizeof(struct thread_info));
		if (tinfo) {
			tinfo->targ.stop_on_error = true;
			if (setup_inputs(dev,&(tinfo->targ.src),opts.inputs,opts.inputs_num))
				free(tinfo);
			else {
				if (process_commands(&(tinfo->tid),&(tinfo->targ),true))
					free(tinfo);
				else {
					tinfo->next = tinfo_list;
					tinfo_list = tinfo;	
				}
			}
		}
	}

	if (opts.interactive_given) {
		struct thread_info *tinfo = calloc (1,sizeof(struct thread_info));
		if (tinfo) {
			tinfo->targ.stop_on_error = false;
			tinfo->targ.verbose = true;
			if (vfi_setup_file(dev,&(tinfo->targ.src),stdin))
				free(tinfo);
			else {
				if (process_commands(&(tinfo->tid),&(tinfo->targ),true))
					free(tinfo);
				else {
					tinfo->next = tinfo_list;
					tinfo_list = tinfo;	
				}
			}
		}
	}

	/* Wait for the spawned children if any */
	while (tinfo_list != NULL) {
		struct thread_info *tinfo = tinfo_list; 
		pthread_join(tinfo->tid,NULL);
		tinfo_list = tinfo_list->next;
		free(tinfo->targ.src); free(tinfo);
	}
	
	vfi_set_dev_done(dev);
	pthread_join(driver_tid,NULL);
	vfi_close(dev);
	cmdline_parser_free(&opts);
	muntrace();

	return 0;
}
