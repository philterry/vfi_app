INCLUDES = $(RDDMA_API_INCLUDES)

bin_PROGRAMS = testprog testprog_async testprog_phil local_dma_test master_sync

testprog_SOURCES = testprog.c tp_cmdline.c tp_cmdline.h

tp_cmdline.h tp_cmdline.c:testprog.ggo
	$(GGO) --file-name=tp_cmdline < testprog.ggo 

testprog_LDADD = $(RDDMA_API_LIBADDS)
testprog_async_LDADD = $(RDDMA_API_LIBADDS)
local_dma_test_LDADD = $(RDDMA_API_LIBADDS)
master_sync_LDADD = $(RDDMA_API_LIBADDS)
testprog_phil_LDADD = $(RDDMA_API_LIBADDS)

EXTRA_DIST = testprog.ggo
