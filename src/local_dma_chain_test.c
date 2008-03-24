#include <vfi_api.h>

/* Simple example app using VFI with chained events.
/* It creates 3 local SMBS, fills one with data, uses the 
 * DMA engine to copy one SMB to another, and compares
 * the source and destination SMB
 */

/* 
 * Build with DBG defined to echo strings sent to and received from VFI.
 * Build with NOTARGET defined to just generate VFI command strings and
 * write them to a file call "junk".
 */
#define DBG
#undef NOTARGET

#ifdef NOTARGET
#define fflush(x) fprintf(x,"\n")
#endif

/*
 * Extract the integer VFI return code from "result(n)"
 */
int get_error_code(char *s) 
{
	int ret;
	char *res;
	if(s)
		res = strstr (s, "result(");
	else
		return 0;

	if (res == NULL) {
		printf("missing result in vfi reply string!\n"
		       "String is: %s\n", s);
		return -1;
	}
	res += 7;
	ret = strtol(res, 0, 10);
	return (ret);
}

/* Send string 'str' to vfi driver.
 * The string will be overwritten by the reply from the driver.
 */
void execute_vfi_string(struct vfi_dev *dev, char *str, char **reply)
{
#ifdef DBG
	printf("%s\n", str);
#endif
#ifdef NOTARGET
	vfi_invoke_cmd_str(dev,str,0);
	*reply = NULL;
#else
 	vfi_do_cmd_str(dev,reply,str,0);
#ifdef DBG
	printf("reply = %s\n", *reply);
#endif
#endif
}

int bind_create(struct vfi_dev *dev,char *name, char *loc, int len, 
	char *destname, char *destloc, int destoff, char *destevent,
	char *srcname, char *srcloc, int srcoff, char *srcevent)
{
	char output[1000];
	char *reply;
	int ret;
	char temp[100];
	sprintf(output,"bind_create://%s", name);

	if (loc) {
		strcat(output,".");
		strcat(output,loc);
	}

	/* Length is required unless user is passing in the entire bind string
	 * through the name field.
	 */
	if (len) {
		sprintf(temp,":%x",len);
		strcat(output,temp);
	}
	else
		goto bind_string_ready;

	strcat(output,"/");
	strcat(output,destname);
	if (destloc) {
		strcat(output,".");
		strcat(output,destloc);
	}

	if (destoff) {
		sprintf(temp,"#%x",destoff);
		strcat(output,temp);
	}

	if (destevent) {
		strcat(output,"?event_name(");
		sprintf(temp,"%s)=",destevent);
		strcat(output,temp);
	}

	strcat(output,srcname);
	if (srcloc) {
		strcat(output,".");
		strcat(output,srcloc);
	}

	if (srcoff) {
		sprintf(temp,"#%x",srcoff);
		strcat(output,temp);
	}

	if (destevent) {
		strcat(output,"?event_name(");
		sprintf(temp,"%s)",destevent);
		strcat(output,temp);
	}
bind_string_ready:
	execute_vfi_string(dev,output,&reply);
	ret = get_error_code(reply);
	free(reply);
	return ret;
}

int event_start(struct vfi_dev *dev, char *name, char *loc, int wait)
{
	char output[1000];
	char *reply;
	int ret;
	sprintf(output,"event_start://%s", name);

	if (loc) {
		strcat(output,".");
		strcat(output,loc);
	}

	execute_vfi_string(dev,output,&reply);

	ret = get_error_code(reply);
	free(reply);
	return ret;
}

int event_chain(struct vfi_dev *dev, char *name, char *loc, char *toname)
{
	char output[1000];
	char *reply;
	int ret;
	sprintf(output,"event_chain://%s", name);

	if (loc) {
		strcat(output, ".");
		strcat(output, loc);
	}

	strcat(output, "?event_name(");
	strcat(output, toname);
	strcat(output, ")");

	execute_vfi_string(dev,output,&reply);

	ret = get_error_code(reply);
	free(reply);
	return ret;
}

int xfer_create(struct vfi_dev *dev, char *name, char *loc)
{
	char output[1000];
	char *reply;
	int ret;
	sprintf(output,"xfer_create://%s", name);

	if (loc) {
		strcat(output,".");
		strcat(output,loc);
	}

	execute_vfi_string(dev,output,&reply);

	ret = get_error_code(reply);
	free(reply);
	return ret;
}

/* Does the VFI smb_mmap, then call libc to actually do the mapping */
int smb_mmap(struct vfi_dev *dev, char *name, char *loc, int offset, int len, void **buf) 
{
	char output[1000];
	char *reply;
	char temp[8];
	char *tid_s;
	unsigned long tid;
	int ret;
	void* mapping;
	sprintf(output,"smb_mmap://%s", name);

	if (loc) {
		strcat(output,".");
		strcat(output,loc);
	}

	if (offset) {
		sprintf(temp,"#%x",offset);
		strcat(output,temp);
	}
	if (len) {
		sprintf(temp,":%x",len);
		strcat(output,temp);
	}

	execute_vfi_string(dev,output,&reply);

	ret = get_error_code(reply);
	if (ret) {
		free(reply);
		return (ret);
	}
#ifdef NOTARGET
	*buf = NULL;
	free(reply);
	return 0;
#endif

	/* Buffer now mmap-able, go for it */
	/*
	* The reply ought to contain an "mmap_offset(x)" term, 
	* where (x) is the offset, in hex, that we need to use
	* with actual mmap calls to map the target area.
	*/
	unsigned long t_id = vfi_get_hex_arg (reply, "mmap_offset");
	printf ("mmap... %08lx\n", t_id);
	mapping = mmap (0, len, PROT_READ | PROT_WRITE, MAP_SHARED, vfi_fileno(dev), t_id);
	if ((unsigned long) mapping == -1) {
		*buf = NULL;
		perror("mmap failed");
	}
	*buf = mapping;
	free(reply);
	return 0;
}

/* 
 * if map==1, *buf is a pointer to the SMB.  Otherwise SMB will be created
 * but not accessible from userland.  
 */
int smb_create(struct vfi_dev *dev, char *name, char *loc, int offset, int len, int map, void **buf) 
{
	char output[1000];
	char *reply;
	char temp[8];
	int ret;
	sprintf(output,"smb_create://%s", name);

	if (loc) {
		strcat(output,".");
		strcat(output,loc);
	}

	if (offset) {
		sprintf(temp,"#%x",offset);
		strcat(output,temp);
	}
	if (len) {
		sprintf(temp,":%x",len);
		strcat(output,temp);
	}

	execute_vfi_string(dev,output,&reply);

	ret = get_error_code(reply);
	free(reply);
	if (ret)
		return (ret);
	else if (!map)
		return 0;

	/* SMB created, now mmap it */
	return (smb_mmap(dev,name, loc, offset, len, buf));
}

/* helper function for building location_create and location_find strings */
void add_opt(char *base, char *option)
{
	int len = strlen(base);

	if (len == 0)
		return;

	if (base[len-1] == ')')
		strcat(base,",");
	else
		strcat(base,"?");

	strcat(base,option);
}

#define SYSROOT 0x1
#define SYSREMOTE 0x2
#define RIO_FABRIC 0x4
#define NET_FABRIC 0x8
#define RIO_DMA 0x10
#define PPC8245_DMA 0x20
#define PRIVATE_OPS 0x100
#define PUBLIC_OPS 0x200

/* flags SYSROOT, SYSREMOTE, RIO_FABRIC, NET_FABRIC, RIO_DMA */
/* Returns status code */
int location_create(struct vfi_dev *dev, char *s, unsigned int flags, int node) 
{
	char output[1000];
	char *reply;
	int ret;
	char temp[8];
#ifdef DBG
	printf("location_create: %s\n", s);
#endif
	sprintf(output,"location_create://%s",s);
	if (flags & SYSROOT) { /* add #1:1 */
		strcat (output, "#1:1");
		node = 0;
	}
	else if (flags & SYSREMOTE) {
		if (node < 2)
			return -1;
		node = 0;
	}

	if (flags & SYSROOT) { /* add #1:1 */
		add_opt(output, "default_ops(private)");
	}
	else if (flags & SYSREMOTE) {
		add_opt(output, "default_ops(public)");
	}

	if (node) {
		sprintf(temp, "#%d",node);
		strcat (output, temp);
	}

	if (flags & RIO_FABRIC) 
		add_opt(output, "fabric(vfi_fabric_rionet)");
	else if (flags & NET_FABRIC) 
		add_opt(output,"fabric(vfi_fabric_net)");

	if (flags & RIO_DMA) 
		add_opt(output, "dma_name(vfi_rio_dma)");
	else if (flags & PPC8245_DMA) 
		add_opt(output,"dma_name(ppc8245)");

	if (!(flags & SYSROOT) && !(flags & SYSREMOTE)) {
		if (flags & PRIVATE_OPS) { /* add #1:1 */
			add_opt(output, "default_ops(private)"); 
		}
		else if (flags & PUBLIC_OPS) {
			add_opt(output, "default_ops(public)");
		}
	}

	execute_vfi_string(dev,output,&reply);

	ret = get_error_code(reply);
	free(reply);
	return ret;
}

#define SMB_LEN 0x3000

/* Create a chain of 4 SMBs. Execute the chain, and compare */
int main (int argc, char **argv)
{

	struct vfi_dev *dev;
	int failed = 0;
	unsigned int *buf1=NULL;
	unsigned int *buf2=NULL;
	unsigned int *buf3=NULL;
	unsigned int *buf4=NULL;
	int i;
	int ret;
	char *output;

	printf("Opening /dev/vfi...");
#ifdef NOTARGET
	dev = calloc(1,sizeof(*dev));
	dev->fd = open("junk",O_RDWR | O_CREAT);
	if ( dev->fd < 0 ) {
		perror("Unable to open junk");
		return (0);
	}
	else
		printf("OK\n");
	dev->file = fdopen(dev->fd,"r+");
	dev->timeout = -1;
#else
	vfi_open(&dev,0,-1);
#endif

	ret = location_create(dev,"fred12", PRIVATE_OPS | RIO_DMA | RIO_FABRIC, 0);
	if (ret) {
		printf("location_create failed\n");
		goto done;
	}

	/* Create and initialize 3 SMBs */

	/* args: name, loc, offset, extent, map */
	ret = smb_create(dev,"buf1", "fred12", 0, SMB_LEN, 1, (void **) &buf1);
	if (ret) {
		printf("smb_create failed\n");
		goto done;
	}
	if (buf1)
		for (i = 0; i < SMB_LEN/4 ; i++)
			buf1[i] = i;

	ret = smb_create(dev,"buf2", "fred12", 0, SMB_LEN, 1, (void **) &buf2);
	if (ret) {
		printf("smb_create failed\n");
		goto done;
	}
	if (buf2)
		for (i = 0; i < SMB_LEN/4 ; i++)
			buf2[i] = 0;

	ret = smb_create(dev,"buf3", "fred12", 0, SMB_LEN, 1, (void **) &buf3);
	if (ret) {
		printf("smb_create failed\n");
		goto done;
	}
	if (buf3)
		for (i = 0; i < SMB_LEN/4 ; i++)
			buf3[i] = 0;

	ret = smb_create(dev,"buf4", "fred12", 0, SMB_LEN, 1, (void **) &buf4);
	if (ret) {
		printf("smb_create failed\n");
		goto done;
	}
	if (buf4)
		for (i = 0; i < SMB_LEN/4 ; i++)
			buf4[i] = 0;


	/* Define DMA engine */
	ret = xfer_create(dev,"xf","fred12");
	if (ret) {
		printf("xfer_create failed\n");
		goto done;
	}

	/* Set up first transfer */
	ret = bind_create(dev,"xf","fred12", SMB_LEN,
			  "buf2", "fred12", 0, "s",	/* dest */
			  "buf1", "fred12", 0, "s");	/* src */
	if (ret) {
		printf("bind_create failed\n");
		goto done;
	}


	/* Set up second transfer */
	ret = bind_create(dev,"xf","fred12", SMB_LEN,
			  "buf3", "fred12", 0, "t",	/* dest */
			  "buf2", "fred12", 0, "t");	/* src */
	if (ret) {
		printf("bind_create failed\n");
		goto done;
	}

	/* Set up third transfer */
	ret = bind_create(dev,"xf","fred12", SMB_LEN,
			  "buf4", "fred12", 0, "u",	/* dest */
			  "buf3", "fred12", 0, "u");	/* src */
	if (ret) {
		printf("bind_create failed\n");
		goto done;
	}



	/* Chain them all together */
	ret = event_chain(dev, "s", "fred12", "t");
	if (ret) {
		printf("event_chain failed\n");
		goto done;
	}

	ret = event_chain(dev, "t", "fred12", "u");
	if (ret) {
		printf("event_chain failed\n");
		goto done;
	}


	/* Run the DMA, only 1 event needed to start this test */
	/* last arg is the wait flag...  block till DMA completes
	 */
	ret = event_start(dev, "s", "fred12", 1);
	if (ret) {
		printf("event_start failed\n");
		goto done;
	}


#ifndef NOTARGET
	/* All done, diff buf1 and buf2 */
	for (i = 0; i < SMB_LEN/4 ; i++) 
		if (buf1[i] != buf2[i]) {
			printf("i(%d), buf1(%d), buf2(%d):  \n", i, buf1[i],buf2[i]);
			failed = 1;
		}

	/* All done, diff buf1 and buf3 */
	for (i = 0; i < SMB_LEN/4 ; i++) 
		if (buf1[i] != buf3[i]) {
			printf("i(%d), buf1(%d), buf3(%d):  \n", i, buf1[i],buf3[i]);
			failed = 1;
		}

	/* All done, diff buf1 and buf4 */
	for (i = 0; i < SMB_LEN/4 ; i++) 
		if (buf1[i] != buf4[i]) {
			printf("i(%d), buf1(%d), buf4(%d):  \n", i, buf1[i],buf4[i]);
			failed = 1;
		}
#endif

	if (failed)
		printf("sorry\n");
	else
		printf("success!\n");
	
done:
	if (buf1)
		munmap(buf1, SMB_LEN);
	if (buf2)
		munmap(buf2, SMB_LEN);
	if (buf3)
		munmap(buf3, SMB_LEN);
	if (buf4)
		munmap(buf4, SMB_LEN);


#if 0
	/* Add a bunch of vfi *_delete calls here... once they're fixed */
	xfer_delete("xf","fred12");  /* assumes xfer_delete gets rid of all binds */
	event_delete("s","fred12");  /* ? */
	event_delete("t","fred12");  /* ? */
	smb_delete("buf4", "fred12");
	smb_delete("buf3", "fred12");
	smb_delete("buf2", "fred12");
	smb_delete("buf1", "fred12");
	location_delete("fred12");
#endif
	vfi_close(dev);
	
}
