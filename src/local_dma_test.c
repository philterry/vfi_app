#define _GNU_SOURCE 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* Simple example app using RDDMA.
/* It creates 2 local SMBS, fills one with data, uses the 
 * DMA engine to copy one SMB to another, and compares
 * the source and destination SMB
 */
int fd_rddma;
FILE *fp_rddma;

/* 
 * Build with DBG defined to echo strings sent to and received from RDDMA.
 * Build with NOTARGET defined to just generate RDDMA command strings and
 * write them to a file call "junk".
 */
#define DBG
#undef NOTARGET

#ifdef NOTARGET
#define fflush(x) fprintf(x,"\n")
#endif

/**
* xtol - convert hex string to long integer
**/
static unsigned long xtol (char *str)
{
	return strtoul(str,0,16);
}

/*
 * Extract the integer RDDMA return code from "result(n)"
 */
int get_error_code(char *s) 
{
	int ret;
	char *res = strcasestr (s, "result(");
#ifdef NOTARGET
	return 0;
#endif
	if (res == NULL) {
		printf("missing result in rddma reply string!\n");
		return -1;
	}
	res += 7;
	ret = strtol(res, 0, 10);
	return (ret);
}

int bind_create(char *name, char *loc, int len, 
	char *destname, char *destloc, int destoff, char *destevent,
	char *srcname, char *srcloc, int srcoff, char *srcevent)
{
	char output[1000];
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
#ifdef DBG
	printf("%s\n", output);
#endif
	fprintf(fp_rddma,output);
	fflush(fp_rddma);
#ifndef NOTARGET
 	fscanf(fp_rddma,"%s", output); 
#ifdef DBG
	printf("%s\n", output);
#endif
#endif
	return (get_error_code(output));
}

int event_start(char *name, char *loc, int wait)
{
	char output[1000];
	sprintf(output,"event_start://%s", name);

	if (loc) {
		strcat(output,".");
		strcat(output,loc);
	}

#ifdef DBG
	printf("%s\n", output);
#endif
	fprintf(fp_rddma,output);
	fflush(fp_rddma);
#ifndef NOTARGET
 	fscanf(fp_rddma,"%s", output); 
#ifdef DBG
	printf("%s\n", output);
#endif
#endif
	return(get_error_code(output));
}

int xfer_create(char *name, char *loc)
{
	char output[1000];
	sprintf(output,"xfer_create://%s", name);

	if (loc) {
		strcat(output,".");
		strcat(output,loc);
	}

#ifdef DBG
	printf("%s\n", output);
#endif
	fprintf(fp_rddma,output);
	fflush(fp_rddma);
#ifndef NOTARGET
 	fscanf(fp_rddma,"%s", output); 
#ifdef DBG
	printf("%s\n", output);
#endif
#endif
	return (get_error_code(output));
}

/* Does the RDDMA smb_mmap, then call libc to actually do the mapping */
int smb_mmap(char *name, char *loc, int offset, int len, void **buf) 
{
	char output[1000];
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
#ifdef DBG
	printf("%s\n", output);
#endif
	fprintf(fp_rddma,output);
	fflush(fp_rddma);
#ifndef NOTARGET
 	fscanf(fp_rddma,"%s", output); 
#ifdef DBG
	printf("%s\n", output);
#endif
#endif
	ret = get_error_code(output);
	if (ret)
		return (ret);
#ifdef NOTARGET
	*buf = NULL;
	return 0;
#endif

	/* Buffer now mmap-able, go for it */
	/*
	* The reply ought to contain an "mmap_offset=<x>" term, 
	* where <x> is the offset, in hex, that we need to use
	* with actual mmap calls to map the target area.
	*/
	tid_s = strcasestr (output, "mmap_offset(");
	unsigned long t_id = xtol (tid_s + 12);
	printf ("mmap... %08lx\n", t_id);
	mapping = mmap (0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_rddma, t_id);
	if ((unsigned long) mapping == -1) {
		*buf == NULL;
		perror("mmap failed");
	}
	*buf = mapping;
	return 0;
}

/* 
 * if map==1, *buf is a pointer to the SMB.  Otherwise SMB will be created
 * but not accessible from userland.  
 */
int smb_create(char *name, char *loc, int offset, int len, int map, void **buf) 
{
	char output[1000];
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
#ifdef DBG
	printf("%s\n", output);
#endif
	fprintf(fp_rddma,output);
	fflush(fp_rddma);
#ifndef NOTARGET
 	fscanf(fp_rddma,"%s", output); 
#ifdef DBG
	printf("%s\n", output);
#endif
#endif
	ret = get_error_code(output);
	if (ret)
		return (ret);
	else if (!map)
		return 0;

	/* SMB created, now mmap it */
	return (smb_mmap(name, loc, offset, len, buf));
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
int location_create(char *s, unsigned int flags, int node) 
{
	char output[1000];
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
		sprintf(temp, "%d",node);
		strcat (output, temp);
	}

	if (flags & RIO_FABRIC) 
		add_opt(output, "fabric(rddma_fabric_rionet)");
	else if (flags & NET_FABRIC) 
		add_opt(output,"fabric(rddma_fabric_net)");

	if (flags & RIO_DMA) 
		add_opt(output, "dma_name(rddma_rio_dma)");
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
#ifdef DBG
	printf("%s\n", output);
#endif
	fprintf(fp_rddma,output);
	fflush(fp_rddma);
#ifndef NOTARGET
 	fscanf(fp_rddma,"%s", output); 
#ifdef DBG
	printf("reply=%s\n",output);
#endif
#endif
	return (get_error_code(output));
}

#define SMB_LEN 0x3000

/* Create 2 local SMBs, copy one to the other, and compare */
int main (int argc, char **argv)
{
	int fd;
	FILE *file;
	int failed = 0;
	unsigned int *buf1;
	unsigned int *buf2;
	int i;
	int ret;

	char *output;

	printf("Opening /dev/rddma...");
#ifdef NOTARGET
	fd_rddma = open("junk",O_RDWR | O_CREAT);
#else
	fd_rddma = open("/dev/rddma",O_RDWR);
#endif
	if ( fd_rddma < 0 ) {
		perror("Unable to open /dev/rddma");
		return (0);
	}
	else
		printf("OK\n");

	fp_rddma = fdopen(fd_rddma,"r+");

	ret = location_create("fred12", PRIVATE_OPS | RIO_DMA | RIO_FABRIC, 0);
	if (ret) {
		printf("location_create failed\n");
		goto done;
	}

	/* Create and initialize 2 SMBs */

	/* args: name, loc, offset, extent, map */
	ret = smb_create("buf1", "fred12", 0, SMB_LEN, 1, (void **) &buf1);
	if (ret) {
		printf("smb_create failed\n");
		goto done;
	}
	if (buf1)
		for (i = 0; i < SMB_LEN/4 ; i++)
			buf1[i] = i;

	ret = smb_create("buf2", "fred12", 0, SMB_LEN, 1, (void **) &buf2);
	if (ret) {
		printf("smb_create failed\n");
		goto done;
	}
	if (buf2)
		for (i = 0; i < SMB_LEN/4 ; i++)
			buf2[i] = 0;

	/* Define DMA engine */
	ret = xfer_create("xf","fred12");
	if (ret) {
		printf("xfer_create failed\n");
		goto done;
	}
	/* Set up transfer */
	ret = bind_create("xf","fred12", SMB_LEN,
		"buf2", "fred12", 0, "s",		/* dest */
		"buf1", "fred12", 0, "s");	/* src */
	if (ret) {
		printf("bind_create failed\n");
		goto done;
	}

	/* Run the DMA, only 1 event needed to start this test */
	/* last arg is the wait flag...  block till DMA completes
	 */
	ret = event_start("s", "fred12", 1);
	if (ret) {
		printf("event_start failed\n");
		goto done;
	}

#ifndef NOTARGET
	/* All done, diff buf1 and buf2 */
	for (i = 0; i < SMB_LEN/4 ; i++) 
		if (buf1[i] != buf2[i]) {
			printf("i, buf1, buf2:  \n", i, buf1[i],buf2[i]);
			failed = 1;
		}
#endif

	if (failed)
		printf("sorry\n");
	else
		printf("success!\n");
	
done:
	fclose(fp_rddma);
	close(fd_rddma);
	
}
