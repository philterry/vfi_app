#include <rddma_api.h>

int main (int argc, char **argv)
{
	int result;
	char *output;
	struct rddma_dev *dev;

	dev = rddma_open(NULL,0);

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
	
	
	sscanf(strstr(output,"result("),"result(%d)",&result);

 	free(output); 

	rddma_close(dev);
	
	return result;
}



