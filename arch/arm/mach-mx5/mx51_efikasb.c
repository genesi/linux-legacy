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
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/flash.h>
#include <linux/regulator/consumer.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <linux/pwm_backlight.h>
#include <linux/pci_ids.h>
#include <linux/suspend.h>
#include <linux/ata.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/keypad.h>
#include <mach/i2c.h>
#include <mach/gpio.h>
#include <mach/mmc.h>
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
extern struct cpu_wp *(*get_cpu_wp)(int *wp);
extern void (*set_num_cpu_wp)(int num);
static int num_cpu_wp = 3;
static int lvds_en_dir = 0;

extern int mxc_get_battery_insertion_status(void);
extern int mxc_get_ac_adapter_insertion_status(void);
extern int mxc_get_batt_low_status(void);
/* extern void mxc_turn_on_batt_low_led(int); */

extern int mxc_get_memory_id(void);
extern unsigned int mxc_get_pcb_id(void);

/* working point(wp): 0 - 800MHz; 1 - 166.25MHz; */
static struct cpu_wp cpu_wp_auto[] = {
	{
	 .pll_rate = 1000000000,
	 .cpu_rate = 1000000000,
	 .pdf = 0,
	 .mfi = 10,
	 .mfd = 11,
	 .mfn = 5,
	 .cpu_podf = 0,
	 .cpu_voltage = 1175000,},
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 800000000,
	 .pdf = 0,
	 .mfi = 8,
	 .mfd = 2,
	 .mfn = 1,
	 .cpu_podf = 0,
	 .cpu_voltage = 1100000,},
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 166250000,
	 .pdf = 4,
	 .mfi = 8,
	 .mfd = 2,
	 .mfn = 1,
	 .cpu_podf = 4,
	 .cpu_voltage = 850000,
	},
};

struct cpu_wp *mx51_efikasb_get_cpu_wp(int *wp)
{
	*wp = num_cpu_wp;
	return cpu_wp_auto;
}

void mx51_efikasb_set_num_cpu_wp(int num)
{
	num_cpu_wp = num;
	return;
}


extern void mx5_ipu_reset(void); /*  eric 20100521 */
static struct mxc_ipu_config mxc_ipu_data = {
	.rev = 2,
	.reset = mx5_ipu_reset,	/* eric 20100521*/
};

extern void mx5_vpu_reset(void);
static struct mxc_vpu_platform_data mxc_vpu_data = {
	.reset = mx5_vpu_reset,
};

extern void mx51_efikasb_gpio_spi_chipselect_active(int cspi_mode, int status,
						    int chipselect);
extern void mx51_efikasb_gpio_spi_chipselect_inactive(int cspi_mode, int status,
						      int chipselect);
static struct mxc_spi_master mxcspi1_data = {
	.maxchipselect = 4,
	.spi_version = 23,
	.chipselect_active = mx51_efikasb_gpio_spi_chipselect_active,
	.chipselect_inactive = mx51_efikasb_gpio_spi_chipselect_inactive,
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

static struct mxc_dvfs_platform_data dvfs_core_data = {
	.reg_id = "SW1",
	.clk1_id = "cpu_clk",
	.clk2_id = "gpc_dvfs_clk",
	.gpc_cntr_reg_addr = MXC_GPC_CNTR,
	.gpc_vcr_reg_addr = MXC_GPC_VCR,
	.ccm_cdcr_reg_addr = MXC_CCM_CDCR,
	.ccm_cacrr_reg_addr = MXC_CCM_CACRR,
	.ccm_cdhipr_reg_addr = MXC_CCM_CDHIPR,
	//	.dvfs_thrs_reg_addr = MXC_DVFSTHRS,
	//.dvfs_coun_reg_addr = MXC_DVFSCOUN,
	//.dvfs_emac_reg_addr = MXC_DVFSEMAC,
	//.dvfs_cntr_reg_addr = MXC_DVFSCNTR,
	.prediv_mask = 0x1F800,
	.prediv_offset = 11,
	.prediv_val = 3,
	.div3ck_mask = 0xE0000000,
	.div3ck_offset = 29,
	.div3ck_val = 2,
	.emac_val = 0x08,
	.upthr_val = 25,
	.dnthr_val = 9,
	.pncthr_val = 33,
	.upcnt_val = 10,
	.dncnt_val = 10,
	.delay_time = 30,
	.num_wp = 3,
};

static struct mxc_dvfsper_data dvfs_per_data = {
	.reg_id = "SW2",
	.clk_id = "gpc_dvfs_clk",
	.gpc_cntr_reg_addr = MXC_GPC_CNTR,
	.gpc_vcr_reg_addr = MXC_GPC_VCR,
	.gpc_adu = 0x0,
	.vai_mask = MXC_DVFSPMCR0_FSVAI_MASK,
	.vai_offset = MXC_DVFSPMCR0_FSVAI_OFFSET,
	.dvfs_enable_bit = MXC_DVFSPMCR0_DVFEN,
	.irq_mask = MXC_DVFSPMCR0_FSVAIM,
	.div3_offset = 0,
	.div3_mask = 0x7,
	.div3_div = 2,
	.lp_high = 1250000,
	.lp_low = 1250000,
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

static void __init mxc_init_fb(void)
{
	mxc_fb_devices[1].num_resources = ARRAY_SIZE(mxcfb_resources);
	mxc_fb_devices[1].resource = mxcfb_resources;
	mxc_register_device(&mxc_fb_devices[1], &fb_data[1]);	/* ron: LVDS LCD */
	mxc_register_device(&mxc_fb_devices[2], NULL);		// Overlay for VPU
}


static void mxc_power_on_lcd(int on) {
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
	.turn_on_backlight = mxc_turn_on_lcd_backlight,
        .power_on_lvds = mxc_power_on_lvds,
        .lvds_enable = mxc_lvds_enable,
};

static struct platform_device mxc_led_device = {
	.name = "efikasb_leds",
	.id = 1,
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

static struct mtd_partition mxc_spi_flash_partitions[] = {
	{
	 .name = "u-boot",
	 .offset = 0x0,
	 .size = SZ_256K,
	},
	{
	  .name = "config",
	  .offset = MTDPART_OFS_APPEND,
	  .size = SZ_64K,
	},
	{
	  .name = "spare",
	  .offset = MTDPART_OFS_APPEND,
	  .size = MTDPART_SIZ_FULL,
	},
};

static struct flash_platform_data mxc_spi_flash_data = {
	.name = "mxc_spi_nor",
	.parts = mxc_spi_flash_partitions,
	.nr_parts = ARRAY_SIZE(mxc_spi_flash_partitions),
	.type = "sst25vf032b",	/* also consider MXIC MX25L3205D later */
};

static struct spi_board_info mxc_spi_board_info[] __initdata = {
	{
	 .modalias = "m25p80",
	 .max_speed_hz = 25000000,
	 .bus_num = 1,
	 .chip_select = 1,
	 .platform_data = &mxc_spi_flash_data,
	},
};

static int sdhc_write_protect(struct device *dev)
{
	unsigned short rc = 0;

	if (to_platform_device(dev)->id == 0)
		rc = gpio_get_value(IOMUX_TO_GPIO(SDHC1_WP_PIN));
	else
		rc = gpio_get_value(IOMUX_TO_GPIO(SDHC2_WP_PIN));

	return rc;
}

static unsigned int sdhc_get_card_det_status(struct device *dev)
{
	int ret;

	if (to_platform_device(dev)->id == 0) {
		ret = gpio_get_value(IOMUX_TO_GPIO(SDHC1_CD_PIN));
		return ret;
	} else {		/* config the det pin for SDHC2 */
		//ron: SDHC2 CD gpio
		ret = gpio_get_value(IOMUX_TO_GPIO(SDHC2_CD_PIN));
		return ret;
	}
}

static struct mxc_mmc_platform_data mmc1_data = {
	.ocr_mask = MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA,
	.min_clk = 400000,
	.max_clk = 52000000,
	.card_inserted_state = 1,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
	.power_mmc = NULL,
};

static struct mxc_mmc_platform_data mmc2_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30 |
	    MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA,
	.min_clk = 150000,
	.max_clk = 50000000,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
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
static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
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

	get_cpu_wp = mx51_efikasb_get_cpu_wp;
	set_num_cpu_wp = mx51_efikasb_set_num_cpu_wp;

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

static struct fsl_ata_platform_data ata_data = {
	.udma_mask = ATA_UDMA3,
	.mwdma_mask = ATA_MWDMA2,
	.pio_mask = ATA_PIO4,
	.fifo_alarm = MXC_IDE_DMA_WATERMARK / 2,
	.max_sg = MXC_IDE_DMA_BD_NR,
	.init = NULL,
	.exit = NULL,
	.core_reg = NULL,
	.io_reg = NULL,
};

static void __init mxc_board_init(void)
{
	struct clk *clk;

	/* SD card detect irqs */
	mxcsdhc2_device.resource[2].start = IOMUX_TO_IRQ(SDHC2_CD_PIN);
	mxcsdhc2_device.resource[2].end = IOMUX_TO_IRQ(SDHC2_CD_PIN);
	mxcsdhc1_device.resource[2].start = IOMUX_TO_IRQ(SDHC1_CD_PIN);
	mxcsdhc1_device.resource[2].end = IOMUX_TO_IRQ(SDHC1_CD_PIN);

	mxc_cpu_common_init();

	mxc_register_gpios();
	mx51_efikasb_io_init();

	clk = clk_get(&(mxc_fb_devices[0].dev), "axi_b_clk");
	clk_set_rate(clk, 133000000);

	mxc_register_device(&mxc_dma_device, NULL);
	mxc_register_device(&mxc_wdt_device, NULL);
	mxc_register_device(&mxcspi1_device, &mxcspi1_data);
#if defined(CONFIG_I2C_MXC) || defined(CONFIG_I2C_MXC_MODULE)
	mxc_register_device(&mxci2c_devices[1], &mx51_efikasb_i2c2_data);
#elif defined(CONFIG_I2C_IMX) || defined(CONFIG_I2C_IMX_MODULE)
	mxc_register_device(&imxi2c2_device, &mx51_efikasb_imxi2c2_data);
#endif
	mxc_register_device(&mxcsdhc1_device, &mmc1_data);
	mxc_register_device(&mxcsdhc2_device, &mmc2_data);

	mxc_register_device(&mxc_rtc_device, &srtc_data);

	mxc_ipu_data.di_clk[1] = clk_get(NULL, "ipu_di1_clk");
	mxc_register_device(&mxc_ipu_device, &mxc_ipu_data);
	mxc_register_device(&mxcvpu_device, &mxc_vpu_data);
	mxc_register_device(&gpu_device, NULL);
	mxc_register_device(&mxcscc_device, NULL);	/* eric 20100521: SCC support */
	mxc_register_device(&mx51_lpmode_device, NULL);
	mxc_register_device(&busfreq_device, NULL);
	mxc_register_device(&sdram_autogating_device, NULL);
	mxc_register_device(&mxc_dvfs_core_device, &dvfs_core_data);
	mxc_register_device(&mxc_dvfs_per_device, &dvfs_per_data);
	mxc_register_device(&mxc_iim_device, NULL);
	mxc_register_device(&mxc_pwm1_device, NULL);

	mxc_register_device(&pata_fsl_device, &ata_data);

	mx51_efikamx_init_audio();

	mxc_init_fb();
	mxc_register_device(&mxc_pwm_backlight_device, &mxc_pwm_backlight_data);
	mxc_register_device(&mxc_led_device, NULL);
	mx51_efikasb_init_mc13892();

	spi_register_board_info(mxc_spi_board_info,
				ARRAY_SIZE(mxc_spi_board_info));

	i2c_register_board_info(1, mxc_i2c1_board_info,
				ARRAY_SIZE(mxc_i2c1_board_info));

	pm_power_off = mxc_power_off;

	mx51_efikamx_init_audio();

	mx5_usb_dr_init();
	mx5_usbh1_init();
	mx51_usbh2_init();
}

static void __init mx51_efikasb_timer_init(void)
{
	struct clk *uart_clk;

	/* Change the CPU voltages for TO2*/
	if (cpu_is_mx51_rev(CHIP_REV_2_0) <= 1) {
		cpu_wp_auto[0].cpu_voltage = 1175000;
		cpu_wp_auto[1].cpu_voltage = 1100000;
		cpu_wp_auto[2].cpu_voltage = 1000000;
	}

	mx51_clocks_init(32768, 24000000, 22579200, 24576000);

	uart_clk = clk_get(NULL, "uart_clk.0");
	early_console_setup(UART1_BASE_ADDR, uart_clk);
}

static struct sys_timer mxc_timer = {
	.init	= mx51_efikasb_timer_init,
};

/*
 * The following uses standard kernel macros define in arch.h in order to
 * initialize __mach_desc_MX51_EFIKASB data structure.
 */
/* *INDENT-OFF* */
MACHINE_START(MX51_EFIKASB, "Genesi Efika MX (Smartbook)")
	/* Maintainer: Genesi, Inc. */
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine = mxc_board_init,
	.timer = &mxc_timer,
MACHINE_END
