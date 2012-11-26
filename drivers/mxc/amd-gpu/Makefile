EXTRA_CFLAGS := \
	-I$(obj)/include \
	-I$(obj)/include/api

obj-$(CONFIG_MXC_KGSL) += kgsl.o
kgsl-objs += 	kgsl_cmdstream.o \
		kgsl_cmdwindow.o \
		kgsl_context.o \
		kgsl_device.o \
		kgsl_drawctxt.o \
		kgsl_driver.o \
		kgsl_g12.o \
		kgsl_intrmgr.o \
		kgsl_memmgr.o \
		kgsl_mmu.o \
		kgsl_ringbuffer.o \
		kgsl_sharedmem.o \
		kgsl_yamato.o \
		kgsl_linux_map.o \
		kgsl_kmod.o \
		kgsl_hal.o \
		kgsl_kmod_cleanup.o \
		kgsl_pwrctrl.o

ifdef CONFIG_KGSL_LOGGING
kgsl-objs += kgsl_log.o
endif
