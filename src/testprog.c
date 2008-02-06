#define _GNU_SOURCE 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct rddma_dev {
	int fd;
	FILE *file;
};

struct rddma_dev *rddma_open()
{
	struct rddma_dev *dev = malloc(sizeof(struct rddma_dev));

	dev->fd = open("/dev/rddma",O_RDWR);

	if ( dev->fd < 0 ) {
		perror("failed");
		free(dev);
		return (0);
	}

	dev->file = fdopen(dev->fd,"r+");

	return dev;
}

void rddma_close(struct rddma_dev *dev)
{
	close(dev->fd);
	free(dev);
}


char *rddma_call(struct rddma_dev *dev, char *cmd)
{
	char *output = NULL;
	fprintf(dev->file,"%s\n",cmd);
	fflush(dev->file);
 	fscanf(dev->file,"%a[^\n]", &output); 
	return output;
}

long rddma_get_hex_option(char *str, char *name)
{
	char *opt;
	char *val;
	if ((opt = strstr(str,name)))
		if ((val = strstr(opt,"(")))
			return strtol(val,0,16);
	return 0;
}

int main (int argc, char **argv)
{
	int result;
	char *output;
	struct rddma_dev *dev;

	dev = rddma_open();

	output = rddma_call(dev,argv[1]);

	/*
	* If the request was "smb_mmap" then use the reply
	* to mmap the region we asked for.
	*/
	if (strcasestr (argv[1], "smb_mmap://")) {
		/*
		* The reply ought to contain an "mmap_offset=<x>" term, 
		* where <x> is the offset, in hex, that we need to use
		* with actual mmap calls to map the target area.
		*/
		unsigned long t_id = rddma_get_hex_option(output,"mmap_offset");
		if (t_id) {
			void* mapping;
			/*
			* Mmap the region and, for giggles, erase its
			* contents.
			*
			*/
			mapping = mmap (0, 512, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, t_id);
			if (mapping && (~((size_t)mapping))) memset (mapping, 0, 512);
		}
	}
	
	
	sscanf(strstr(output,"result("),"result(%d)",&result);

 	free(output); 

	rddma_close(dev);
	
	return result;
}



