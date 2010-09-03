#
# Makefile for the linux kernel.
#

# Object file lists.


obj-y   := system.o iomux.o cpu.o mm.o devices.o serial.o dma.o lpmodes.o pm.o \
sdram_autogating.o bus_freq.o usb_dr.o usb_h1.o usb_h2.o dummy_gpio.o wfi.o suspend.o early_setup.o

obj-$(CONFIG_ARCH_MX51) += clock.o
obj-$(CONFIG_ARCH_MX53) += clock.o
obj-$(CONFIG_ARCH_MX50) += clock_mx50.o

obj-$(CONFIG_MACH_MX51_3DS) += mx51_3stack.o mx51_3stack_gpio.o mx51_3stack_pmic_mc13892.o
obj-$(CONFIG_MACH_MX51_BABBAGE) += mx51_babbage.o mx51_babbage_gpio.o mx51_babbage_pmic_mc13892.o
obj-$(CONFIG_MACH_MX53_EVK) += mx53_evk.o mx53_evk_gpio.o mx53_evk_pmic_mc13892.o
obj-$(CONFIG_MACH_MX50_ARM2) += mx50_arm2.o mx50_arm2_gpio.o mx50_arm2_pmic_mc13892.o

obj-$(CONFIG_MACH_MX51_EFIKAMX) += mx51_efikamx.o mx51_efikamx_io.o mx51_efikamx_cpu.o \
				   mx51_efikamx_uart.o mx51_efikamx_usb.o mx51_efikamx_i2c.o \
				   mx51_efikamx_spi.o mx51_efikamx_nor.o mx51_efikamx_pmic.o \
				   mx51_efikamx_mmc.o mx51_efikamx_ata.o mx51_efikamx_powerkey.o \
				   mx51_efikamx_audio.o mx51_efikamx_display.o early_setup.o
obj-$(CONFIG_MACH_MX51_EFIKASB) += mx51_efikasb.o mx51_efikasb_gpio.o mx51_efikasb_pmic.o \
                                mx51_efikasb_idle_timer.o mx51_efikasb_input.o \
                                mx51_efikasb_lid.o mx51_efikasb_powerkey.o
