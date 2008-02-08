#include <rddma_api.h>

#define MAX_THR 8

int main (int argc, char **argv)
{
	aio_context_t ctx = 0;
	int efd;
	struct rddma_dev *dev;
	struct iocb *iocb;
	struct timespec tmo;

	if ((efd = rddma_get_eventfd(0)) < 0)
		return efd;

	dev = rddma_open();

}
