EXTRA_CFLAGS := \
	-I$(obj)/include \
	-I$(obj)/include/api

obj-$(CONFIG_MXC_AMD_GPU) += gpu.o
gpu-objs += 	common/gsl_cmdstream.o \
		common/gsl_cmdwindow.o \
		common/gsl_context.o \
		common/gsl_debug_pm4.o \
		common/gsl_device.o \
		common/gsl_drawctxt.o \
		common/gsl_driver.o \
		common/gsl_g12.o \
		common/gsl_intrmgr.o \
		common/gsl_memmgr.o \
		common/gsl_mmu.o \
		common/gsl_ringbuffer.o \
		common/gsl_sharedmem.o \
		common/gsl_yamato.o \
		common/gsl_linux_map.o \
		common/gsl_kmod.o \
		common/gsl_hal.o \
		common/gsl_kmod_cleanup.o \
		common/misc.o \


ifdef CONFIG_MXC_AMD_GPU_LOGGING
gpu-objs += common/gsl_log.o
EXTRA_CFLAGS += -DGSL_LOG
endif
