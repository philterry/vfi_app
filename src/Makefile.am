INCLUDES = $(RDDMA_API_CFLAGS)

LIBS = $(RDDMA_API_LIBS) -lpthread

bin_PROGRAMS = testprog testprog_async testprog_phil local_dma_test master_sync

testprog_SOURCES = testprog.c tp_cmdline.c tp_cmdline.h

tp_cmdline.h tp_cmdline.c:testprog.ggo
	$(GGO) --file-name=tp_cmdline < testprog.ggo 

EXTRA_DIST = testprog.ggo
