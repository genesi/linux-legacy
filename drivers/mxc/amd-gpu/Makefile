EXTRA_CFLAGS := \
	-I$(obj)/include \
	-I$(obj)/include/api

obj-$(CONFIG_MXC_AMD_GPU) += gpu.o
gpu-objs += 	gsl_cmdstream.o \
		gsl_cmdwindow.o \
		gsl_context.o \
		gsl_device.o \
		gsl_drawctxt.o \
		gsl_driver.o \
		gsl_g12.o \
		gsl_intrmgr.o \
		gsl_memmgr.o \
		gsl_mmu.o \
		gsl_ringbuffer.o \
		gsl_sharedmem.o \
		gsl_yamato.o \
		gsl_linux_map.o \
		gsl_kmod.o \
		gsl_hal.o \
		gsl_kmod_cleanup.o \
		misc.o

ifdef CONFIG_MXC_AMD_GPU_LOGGING
gpu-objs += gsl_log.o
EXTRA_CFLAGS += -DGSL_LOG
endif
