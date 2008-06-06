INCLUDES = $(VFI_API_CFLAGS) $(VFI_FRMWRK_CFLAGS)

LIBS = $(VFI_FRMWRK_LIBS) $(VFI_API_LIBS) -lpthread -lrt

bin_PROGRAMS = testprog testprog_async frmwrk local_dma_test master_sync local_dma_chain_test

testprog_SOURCES = testprog.c tp_cmdline.c tp_cmdline.h
frmwrk_SOURCES = frmwrk.c fw_cmdline.c fw_cmdline.h

%.h %.c:%.ggo
	$(GGO) --file-name=$* < $< 

