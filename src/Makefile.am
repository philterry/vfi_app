INCLUDES = $(RDDMA_API_INCLUDES)

bin_PROGRAMS = testprog testprog_async testprog_phil local_dma_test

testprog_SOURCES = testprog.c cmdline.c cmdline.h

cmdline.h:testprog.ggo
	$(GGO) < testprog.ggo

testprog_LDADD = $(RDDMA_API_LIBADDS)
testprog_async_LDADD = $(RDDMA_API_LIBADDS)
local_dma_test_LDADD = $(RDDMA_API_LIBADDS)
testprog_phil_LDADD = $(RDDMA_API_LIBADDS)

EXTRA_DIST = testprog.ggo
