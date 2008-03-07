INCLUDES = $(RDDMA_API_CFLAGS)

LIBS = $(RDDMA_API_LIBS) -lpthread

bin_PROGRAMS = testprog testprog_async frmwrk local_dma_test master_sync

testprog_SOURCES = testprog.c tp_cmdline.c tp_cmdline.h
frmwrk_SOURCES = frmwrk.c fw_cmdline.c fw_cmdline.h

%.h %.c:%.ggo
	$(GGO) --file-name=$* < $< 

EXTRA_DIST = testprog.ggo
