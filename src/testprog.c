#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>


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
 	free(output); 

	close(fd);
	
}
