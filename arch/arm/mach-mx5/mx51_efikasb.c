/*
 * Copyright 2009-2010 Pegatron Corporation. All Rights Reserved.
 * Copyright 2009-2010 Genesi USA, Inc. All Rights Reserved.
 *
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <linux/pwm_backlight.h>
#include <linux/pci_ids.h>
#include <linux/suspend.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/keypad.h>
#include <mach/i2c.h>
#include <mach/gpio.h>
#include <mach/mxc_dvfs.h>

#include "devices.h"
#include "mx51_efikasb.h"
#include "iomux.h"
#include "mx51_pins.h"
#include "crm_regs.h"
#include "usb.h"
#include <mach/clock.h>

#include <linux/sbs.h>

#if defined(CONFIG_I2C_MXC) && defined(CONFIG_I2C_IMX)
#error pick CONFIG_I2C_MXC or CONFIG_I2C_IMX but not both, please..
#endif


/*!
 * @file mach-mx51/mx51_efikasb.c
 *
 * @brief This file contains the board specific initialization routines.
 *
 * @ingroup MSL_MX51
 */
extern void __init mx51_efikasb_io_init(void);
static int lvds_en_dir = 0;

extern int mxc_get_battery_insertion_status(void);
extern int mxc_get_ac_adapter_insertion_status(void);
extern int mxc_get_batt_low_status(void);

extern int mxc_get_memory_id(void);
extern unsigned int mxc_get_pcb_id(void);

extern void mx5_ipu_reset(void); /*  eric 20100521 */
static struct mxc_ipu_config mxc_ipu_data = {
	.rev = 2,
	.reset = mx5_ipu_reset,	/* eric 20100521*/
};

extern void mx5_vpu_reset(void);
static struct mxc_vpu_platform_data mxc_vpu_data = {
	.reset = mx5_vpu_reset,
};

#if defined(CONFIG_I2C_MXC)
static struct mxc_i2c_platform_data mx51_efikasb_i2c2_data = {
	.i2c_clk = 100000,
};
#elif defined(CONFIG_I2C_IMX)
static struct imxi2c_platform_data mx51_efikasb_imxi2c2_data = {
	.bitrate = 100000,
};

static struct resource imxi2c2_resources[] = {
	{
		.start = I2C2_BASE_ADDR,
		.end = I2C2_BASE_ADDR + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = MXC_INT_I2C2,
		.end = MXC_INT_I2C2,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device imxi2c2_device = {
	.name = "imx-i2c",
	.id = 1,
	.resource = imxi2c2_resources,
	.num_resources = ARRAY_SIZE(imxi2c2_resources),
};
#endif


static struct mxc_srtc_platform_data srtc_data = {
	.srtc_sec_mode_addr = 0x83F98840,
};

static struct resource mxcfb_resources[] = {
	[0] = {
	       .flags = IORESOURCE_MEM,
	       },
};

static struct mxc_fb_platform_data fb_data[] = {
	[1] = {
	 .interface_pix_fmt = IPU_PIX_FMT_RGB565,
	},

};

static void __init mx51_efikasb_init_display(void)
{
	/* what in the hell does this do? */
	clk_set_rate(clk_get(&(mxc_fb_devices[0].dev), "axi_b_clk"), 133000000);

	mxc_ipu_data.di_clk[1] = clk_get(NULL, "ipu_di1_clk");
	mxc_register_device(&mxc_ipu_device, &mxc_ipu_data);
	mxc_register_device(&mxcvpu_device, &mxc_vpu_data);
	mxc_register_device(&gpu_device, NULL);

	mxc_fb_devices[1].num_resources = ARRAY_SIZE(mxcfb_resources);
	mxc_fb_devices[1].resource = mxcfb_resources;
	mxc_register_device(&mxc_fb_devices[1], &fb_data[1]);	/* ron: LVDS LCD */
	mxc_register_device(&mxc_fb_devices[2], NULL);		// Overlay for VPU
}


static void mxc_power_on_lcd(int on)
{
        if(on) {
                gpio_set_value(IOMUX_TO_GPIO(LCD_PWRON_PIN), 1);        /* LCD Power On */
        } else {
                gpio_set_value(IOMUX_TO_GPIO(LCD_PWRON_PIN), 0);        /* LCD Power Off */
        }

}

static void mxc_lvds_enable(int on)
{
        if(on == -1) {
                gpio_direction_input(IOMUX_TO_GPIO(LCD_LVDS_EN_PIN));
                lvds_en_dir = 1;
                return;
        }

        lvds_en_dir = 0;
        if(on) {
                gpio_set_value(IOMUX_TO_GPIO(LCD_LVDS_EN_PIN), 1);        /* LVDS_EN On */
        } else {
                gpio_set_value(IOMUX_TO_GPIO(LCD_LVDS_EN_PIN), 0);        /* LVDS_EN Off */
        }
}

static void mxc_turn_on_lcd_backlight(int on)
{
        if(on) {
                mxc_free_iomux(LCD_BL_PWM_PIN, IOMUX_CONFIG_GPIO);
                mxc_request_iomux(LCD_BL_PWM_PIN, IOMUX_CONFIG_ALT1);

                msleep(10);

                gpio_set_value(IOMUX_TO_GPIO(LCD_BL_PWRON_PIN), 0);     /* Backlight Power On */
        } else {
                gpio_set_value(IOMUX_TO_GPIO(LCD_BL_PWRON_PIN), 1);     /* Backlight Power Off */

                msleep(10);

                mxc_free_iomux(LCD_BL_PWM_PIN, IOMUX_CONFIG_ALT1);
                mxc_request_iomux(LCD_BL_PWM_PIN, IOMUX_CONFIG_GPIO);
                gpio_direction_output(IOMUX_TO_GPIO(LCD_BL_PWM_PIN), 0);
        }
}

static struct platform_pwm_backlight_data mxc_pwm_backlight_data = {
	.pwm_id = 0,
	.max_brightness = 64,
	.dft_brightness = 32,
	.pwm_period_ns = 78770,
	.power = mxc_turn_on_lcd_backlight,
};


static void mxc_reset_lvds(void)
{
        gpio_set_value(IOMUX_TO_GPIO(LVDS_RESET_PIN), 0);
        msleep(50);
        gpio_set_value(IOMUX_TO_GPIO(LVDS_RESET_PIN), 1);
        msleep(10);
        gpio_set_value(IOMUX_TO_GPIO(LVDS_RESET_PIN), 0);

}

static void mxc_power_on_lvds(int on)
{
        if(on)
                gpio_set_value(IOMUX_TO_GPIO(LVDS_PWRCTL_PIN), 1);
        else
                gpio_set_value(IOMUX_TO_GPIO(LVDS_PWRCTL_PIN), 0);
}

static struct mxc_lcd_platform_data lvds_data = {
        .core_reg = "VCAM",
        .io_reg = "VGEN3",
        .analog_reg = "VAUDIO",
        .reset = mxc_reset_lvds,
	.power_on_lcd = mxc_power_on_lcd,
        .power_on_lvds = mxc_power_on_lvds,
        .lvds_enable = mxc_lvds_enable,
};

enum mxc_power_resource {
	MAINS_INSERTION_STATUS,
	BATTERY_INSERTION_STATUS,
	BATTERY_ALERT,
};

static struct resource mxc_power_resources[] = {
	[MAINS_INSERTION_STATUS] = {
		.start = IOMUX_TO_IRQ(AC_ADAP_INS_PIN),
		.end   = IOMUX_TO_IRQ(AC_ADAP_INS_PIN),
		.name  = "mains-insertion-status",
		.flags =  IORESOURCE_IRQ
			| IORESOURCE_IRQ_LOWEDGE
			| IORESOURCE_IRQ_HIGHEDGE,
	},
	[BATTERY_INSERTION_STATUS] = {
		.start = IOMUX_TO_IRQ(BATT_INS_PIN),
		.end   = IOMUX_TO_IRQ(BATT_INS_PIN),
		.name  = "battery-insertion-status",
		.flags =  IORESOURCE_IRQ
			| IORESOURCE_IRQ_LOWEDGE
			| IORESOURCE_IRQ_HIGHEDGE,
	},
	[BATTERY_ALERT] = {
		.start = IOMUX_TO_IRQ(BATT_LOW_PIN),
		.end   = IOMUX_TO_IRQ(BATT_LOW_PIN),
		.name  = "battery-alert",
		.flags =  IORESOURCE_IRQ
			| IORESOURCE_IRQ_LOWEDGE
			| IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct sbs_platform_data sbs_platform_data = {
	.mains_insertion_status   = mxc_get_ac_adapter_insertion_status,
	.battery_insertion_status = mxc_get_battery_insertion_status,

	.battery_alert            = &mxc_power_resources[BATTERY_ALERT],

	.mains_presence_changed   = &mxc_power_resources[MAINS_INSERTION_STATUS],
	.battery_presence_changed = &mxc_power_resources[BATTERY_INSERTION_STATUS],
};

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
	{
		.type = "sgtl5000-i2c",
		.addr = 0x0a,
	},
	{	/* ron: LVDS Controller */
		.type = "mtl017",
		.addr = 0x3a,
		.platform_data = &lvds_data,
	},
	{
		.type          = "smart-battery",
		.addr          = 0x0b,
		.platform_data = &sbs_platform_data,
	},
};

/*!
 * Board specific fixup function. It is called by \b setup_arch() in
 * setup.c file very early on during kernel starts. It allows the user to
 * statically fill in the proper values for the passed-in parameters. None of
 * the parameters is used currently.
 *
 * @param  desc         pointer to \b struct \b machine_desc
 * @param  tags         pointer to \b struct \b tag
 * @param  cmdline      pointer to the command line
 * @param  mi           pointer to \b struct \b meminfo
 */
static void __init mx51_efikasb_fixup(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	char *str;
	struct tag *t;
	struct tag *mem_tag = 0;
	int total_mem = SZ_512M;
	int left_mem = 0;
	int gpu_mem = SZ_64M;
	int fb_mem = SZ_32M;

	mxc_set_cpu_type(MXC_CPU_MX51);

	get_cpu_wp = mx51_efikamx_get_cpu_wp;
	set_num_cpu_wp = mx51_efikamx_set_num_cpu_wp;

	for_each_tag(mem_tag, tags) {
		if (mem_tag->hdr.tag == ATAG_MEM) {
			total_mem = mem_tag->u.mem.size;
			left_mem = total_mem - gpu_mem - fb_mem;
			break;
		}
	}

	for_each_tag(t, tags) {
		if (t->hdr.tag == ATAG_CMDLINE) {
			str = t->u.cmdline.cmdline;
			str = strstr(str, "mem=");
			if (str != NULL) {
				str += 4;
				left_mem = memparse(str, &str);
				if (left_mem == 0 || left_mem > total_mem)
					left_mem = total_mem - gpu_mem - fb_mem;
			}

			str = t->u.cmdline.cmdline;
			str = strstr(str, "gpu_memory=");
			if (str != NULL) {
				str += 11;
				gpu_mem = memparse(str, &str);
			}

			break;
		}
	}

	if (mem_tag) {
		fb_mem = total_mem - left_mem - gpu_mem;
		if (fb_mem < 0) {
			gpu_mem = total_mem - left_mem;
			fb_mem = 0;
		}
		mem_tag->u.mem.size = left_mem;

		/*reserve memory for gpu*/
		gpu_device.resource[5].start =
				mem_tag->u.mem.start + left_mem;
		gpu_device.resource[5].end =
				gpu_device.resource[5].start + gpu_mem - 1;

		if (fb_mem) {
			mxcfb_resources[0].start =
				gpu_device.resource[5].end + 1;
			mxcfb_resources[0].end =
				mxcfb_resources[0].start + fb_mem - 1;
		} else {
			mxcfb_resources[0].start = 0;
			mxcfb_resources[0].end = 0;
		}
	}
}

#define PWGT1SPIEN (1<<15)
#define PWGT2SPIEN (1<<16)
#define USEROFFSPI (1<<3)

static void mxc_power_off(void)
{
        mxc_turn_on_lcd_backlight(0);

        msleep(200);

        if(lvds_en_dir == 0) {
                mxc_lvds_enable(0);
        }

        mxc_power_on_lvds(0);

        msleep(5);

        mxc_power_on_lcd(0);

	/* We can do power down one of two ways:
	   Set the power gating
	   Set USEROFFSPI */
        gpio_set_value(IOMUX_TO_GPIO(USB_PHY_RESET_PIN), 0);
        msleep(10);
	/* Set the power gate bits to power down */
	pmic_write_reg(REG_POWER_MISC, (PWGT1SPIEN|PWGT2SPIEN),
		(PWGT1SPIEN|PWGT2SPIEN));

        mxc_wd_reset();

	//robin: CLR_DFF
	mxc_request_iomux(SYS_PWROFF_PIN, IOMUX_CONFIG_GPIO);
	gpio_direction_output(IOMUX_TO_GPIO(SYS_PWROFF_PIN), 0);
	gpio_set_value(IOMUX_TO_GPIO(SYS_PWROFF_PIN), 0);
	msleep(10);
	gpio_set_value(IOMUX_TO_GPIO(SYS_PWROFF_PIN), 1);
}

static irqreturn_t wwan_wakeup_int(int irq, void *dev_id)
{
	if(gpio_get_value(IOMUX_TO_GPIO(WWAN_WAKEUP_PIN)))
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	else
		set_irq_type(irq, IRQF_TRIGGER_RISING);

	pr_info("WWAN wakeup event\n");
	return IRQ_HANDLED;
}

static int __init mxc_init_wwan_wakeup(void)
{
	int irq, ret;

	irq = IOMUX_TO_IRQ(WWAN_WAKEUP_PIN);

	if(gpio_get_value(IOMUX_TO_GPIO(WWAN_WAKEUP_PIN)))
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	else
		set_irq_type(irq, IRQF_TRIGGER_RISING);

	ret = request_irq(irq, wwan_wakeup_int, 0, "wwan-wakeup", 0);
	if(ret)
		pr_info("register WWAN wakeup interrupt failed\n");
	else
		enable_irq_wake(irq);

	return ret;

}
late_initcall(mxc_init_wwan_wakeup);

int mx51_efikamx_revision(void)
{
	return 0;
}

static void __init mx51_efikasb_board_init(void)
{
	mxc_cpu_common_init();

	mxc_register_gpios();
	mx51_efikasb_io_init();

	mx51_efikamx_init_uart();
	mx51_efikamx_init_soc();

	mx51_efikasb_init_pmic();
	mx51_efikamx_init_nor();
	mx51_efikamx_init_spi();

#if defined(CONFIG_I2C_MXC) || defined(CONFIG_I2C_MXC_MODULE)
	mxc_register_device(&mxci2c_devices[1], &mx51_efikasb_i2c2_data);
#elif defined(CONFIG_I2C_IMX) || defined(CONFIG_I2C_IMX_MODULE)
	mxc_register_device(&imxi2c2_device, &mx51_efikasb_imxi2c2_data);
#endif

	mxc_register_device(&mxc_rtc_device, &srtc_data);

	mx51_efikamx_init_mmc();
	mx51_efikamx_init_pata();

	mxc_register_device(&mxc_pwm1_device, NULL);
	mxc_register_device(&mxc_pwm_backlight_device, &mxc_pwm_backlight_data);

	mx51_efikasb_init_display();

	mx51_efikasb_init_leds();

	i2c_register_board_info(1, mxc_i2c1_board_info,
				ARRAY_SIZE(mxc_i2c1_board_info));

	pm_power_off = mxc_power_off;

	mx51_efikamx_init_audio();

	mx5_usb_dr_init();
	mx5_usbh1_init();
	mx51_usbh2_init();
}

static struct sys_timer mx51_efikasb_timer = {
	.init	= mx51_efikamx_timer_init,
};

MACHINE_START(MX51_EFIKASB, "Genesi Efika MX (Smartbook)")
	/* Maintainer: Genesi, Inc. */
	.fixup = mx51_efikasb_fixup,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine = mx51_efikasb_board_init,
	.timer = &mx51_efikasb_timer,
MACHINE_END
