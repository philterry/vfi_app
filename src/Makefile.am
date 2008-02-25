bin_PROGRAMS = testprog testprog_async testprog_phil local_dma_test

testprog_SOURCES = testprog.c cmdline.c cmdline.h

cmdline.h:testprog.ggo
	gengetopt < testprog.ggo

testprog_LDADD = -lrddma_api -lpthread
testprog_async_LDADD = -L/home/phil/vinstall/lib -lrddma_api -lpthread
local_dma_test_LDADD = -L/home/phil/vinstall/lib -lrddma_api -lpthread
testprog_phil_LDADD = -L/home/phil/vinstall/lib -lrddma_api -lpthread

INCLUDES = -I/home/phil/vinstall/include
