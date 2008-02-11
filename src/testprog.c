#define _GNU_SOURCE 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static unsigned long xtol (char *str);

int main (int argc, char **argv)
{
	int fd;
	FILE *file;

	char *output;

	printf("Opening /dev/rddma...");
	fd = open("/dev/rddma",O_RDWR);
	if ( fd < 0 ) {
		perror("failed");
		return (0);
	}
	else
		printf("OK\n");

	file = fdopen(fd,"r+");

	printf("Writing \"%s\" to /dev/rddma...",argv[1]);
	fprintf(file,"%s\n",argv[1]);
	fflush(file);
	printf("Done\n");
	
	printf("Reading /dev/rddma\n");

 	fscanf(file,"%a[^\n]", &output); 
	printf("\"%s\"\n",output);
	
	
	/*
	* If the request was "smb_mmap" then use the reply
	* to mmap the region we asked for.
	*/
	if (strcasestr (argv[1], "smb_mmap")) {
		/*
		* The reply ought to contain an "mmap_offset=<x>" term, 
		* where <x> is the offset, in hex, that we need to use
		* with actual mmap calls to map the target area.
		*/
		char *tid_s = strcasestr (output, "mmap_offset(");
		if (tid_s) {
			void* mapping;
			unsigned long t_id = xtol (tid_s + 12);
			printf ("mmap... %08lx\n", t_id);
			/*
			* Mmap the region and, for giggles, erase its
			* contents.
			*
			*/
			mapping = mmap (0, 512, PROT_READ | PROT_WRITE, MAP_SHARED, fd, t_id);
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
	
 	free(output); 

	close(fd);
	
}

/**
* xtol - convert hex string to long integer
*
* @str: string to convert.
*
* This function converts hex digits at a specified string into an unsigned long.
* It is quite primitive, and will keep converting until the string exhausts or
* some non-hex character is encountered. It doesn't check the answer; it just 
* doesn't care. It *will* allow a leading "0x".
*
* I'm sure there is already a perfectly good C-library function that does exactly
* thins, but I can't find one and I'm buggered if I can remember what it might 
* be called.
*
**/
static unsigned long xtol (char *str)
{
	return strtoul(str,0,16);
}


