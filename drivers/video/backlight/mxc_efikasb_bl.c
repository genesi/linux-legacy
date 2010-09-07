/*
 * Copyright 2007 Pegatron Corp, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
/*!
 * @defgroup LCDC_BL MXC LCDC Backlight Driver
 */
/*!
 * @file mxc_efikasb_bl.c
 *
 * @brief Backlight Driver for LCDC PWM on Genesi Efika MX Smartbook.
 *
 * This file contains API defined in include/linux/clk.h for setting up and
 * retrieving clocks.
 *
 * Based on Sharp's Corgi Backlight Driver
 *
 * @ingroup LCDC_BL
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/clk.h>
#include <mach/hardware.h>

#define MXC_MAX_BL_LEVEL          11
#define MXC_MIN_BL_LEVEL          0
#define MXC_DFL_BL_LEVEL          6

#define MXC_MAX_INTENSITY 	255
#define MXC_DEFAULT_INTENSITY 	127
#define MXC_INTENSITY_OFF 	0
#define MXC_PWM_PERIOD_NS	78770

#define MXC_PWMCR		(0x00)
#define MXC_PWMSR		(0x04)
#define MXC_PWMIR		(0x08)
#define MXC_PWMSAR		(0x0C)
#define MXC_PWMPR		(0x10)
#define MXC_PWMCNR		(0x14)

#define MXC_PWMCR_EN			(1)
#define MXC_PWMCR_PRESCALER_MASK	(0xFFFF000F)
#define MXC_PWMCR_PRESCALER_OFFSET	(4)
#define MXC_PWMCR_CLKSRC_OFF		(0)
#define MXC_PWMCR_CLKSRC_IPG		(1 << 16)
#define MXC_PWMCR_CLKSRC_IPG_HIGH	(2 << 16)
#define MXC_PWMCR_CLKSRC_32K		(3 << 16)
#define MXC_PWMCR_STOPEN		(1 << 25)
#define MXC_PWMCR_DOZEEN                (1 << 24)
#define MXC_PWMCR_WAITEN                (1 << 23)
#define MXC_PWMCR_DBGEN			(1 << 22)

#define MXC_PWMSAR_SAMPLE_MASK		(0xFFFF0000)
#define MXC_PWMPR_PERIOD_MASK		(0xFFFF0000)

#define MXC_PWMCR_PRESCALER(x)    (((x - 1) & 0xFFF) << 4)

int bl_level_to_pwm_map[MXC_MAX_BL_LEVEL] = {
        0, 4, 25, 63, 75, 101, 127, 153, 179, 205, 231, 255,
};

static BLOCKING_NOTIFIER_HEAD(backlight_notifier_list);

int register_backlight_notifier(struct notifier_block *nb)
{
        return blocking_notifier_chain_register(&backlight_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(register_backlight_notifier);

int unregister_backlight_notifier(struct notifier_block *nb)
{
        return blocking_notifier_chain_unregister(&backlight_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_backlight_notifier);

struct mxcbl_dev_data {
	void *base;
	struct clk *clk;
	int clk_enabled;
	int intensity;
	int suspend;
};

static int mxc_pwm_enable(struct mxcbl_dev_data *mxc_bl)
{
	unsigned long reg;
	int rc = 0;

	if (!mxc_bl->clk_enabled) {
		rc = clk_enable(mxc_bl->clk);
		if (!rc)
			mxc_bl->clk_enabled = 1;
	}

	reg = __raw_readl(mxc_bl->base + MXC_PWMCR);
	reg |= MXC_PWMCR_EN;
	__raw_writel(reg, mxc_bl->base + MXC_PWMCR);
	return rc;
}

static void mxc_pwm_disable(struct mxcbl_dev_data *mxc_bl)
{
	unsigned long reg;

	if (mxc_bl->clk_enabled) {
		clk_disable(mxc_bl->clk);
		mxc_bl->clk_enabled = 0;
	}

	reg = __raw_readl(mxc_bl->base + MXC_PWMCR);
	reg &= ~MXC_PWMCR_EN;
	__raw_writel(reg, mxc_bl->base + MXC_PWMCR);

}

void mxc_set_brightness(struct mxcbl_dev_data *mxc_bl, uint8_t level)
{
	unsigned long long c;
	unsigned long period_cycles, duty_cycles, prescale;
	int duty_ns, period_ns;
	
	//printk("mxc_set_brightness: level=%x\n", level);

#if 0
 	period_ns = MXC_PWM_PERIOD_NS; 
 	duty_ns = level * period_ns / MXC_MAX_INTENSITY; 
	
 	c = clk_get_rate(mxc_bl->clk) * 665 / 80; 
 	c = c * period_ns; 
 	do_div(c, 1000000000); 
 	period_cycles = c; 
	
 	prescale = period_cycles / 0x10000 + 1; 

 	period_cycles /= prescale; 
 	c = (unsigned long long)period_cycles * duty_ns; 
 	do_div(c, period_ns); 
 	duty_cycles = c; 
#else
	/* ron: hard code the PWM configuration */
	prescale = 300;
	period_cycles = 0x03f1;
	duty_cycles = bl_level_to_pwm_map[level] * period_cycles / MXC_MAX_INTENSITY;
#endif
        //printk("duty_cycles %d, period_cycles %d\n", duty_cycles, period_cycles);
	__raw_writel(duty_cycles, mxc_bl->base + MXC_PWMSAR);
	__raw_writel(period_cycles, mxc_bl->base + MXC_PWMPR);
	__raw_writel(MXC_PWMCR_PRESCALER(prescale) |
		     MXC_PWMCR_CLKSRC_IPG |
		     MXC_PWMCR_STOPEN | MXC_PWMCR_DOZEEN |
		     MXC_PWMCR_WAITEN | MXC_PWMCR_DBGEN | MXC_PWMCR_EN,
		     mxc_bl->base + MXC_PWMCR);
}

#define BL_BRIGHTNESS  0x01

static int mxcbl_set_brightness(struct backlight_device *bd)
{
        int level = bd->props.brightness;
        struct mxcbl_dev_data *devdata = dev_get_drvdata(&bd->dev);

        if (level > MXC_MAX_BL_LEVEL)
                return 0;
        
	if (bd->props.power != FB_BLANK_UNBLANK)
		level = 0;
	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		level = 0;

	if (level && !devdata->clk_enabled)
		mxc_pwm_enable(devdata);

	mxc_set_brightness(devdata, level);

	if (level == 0 && devdata->clk_enabled)
		mxc_pwm_disable(devdata);

	devdata->intensity = bl_level_to_pwm_map[level];
        blocking_notifier_call_chain(&backlight_notifier_list, BL_BRIGHTNESS, &level);

	return 0;
}

static int mxcbl_get_brightness(struct backlight_device *bd)
{
	struct mxcbl_dev_data *devdata = dev_get_drvdata(&bd->dev);
        printk("intensity=%d\n", devdata->intensity);
	return bd->props.brightness;
}

static int mxcbl_check_fb(struct fb_info *info)
{

	if (strcmp(info->fix.id, "DISP0 BG") == 0) {
		return 1;
	}
	return 0;
}

static struct backlight_ops mxcbl_ops = {
	.get_brightness = mxcbl_get_brightness,
	.update_status = mxcbl_set_brightness,
	.check_fb = mxcbl_check_fb,
};

static int __init mxcbl_probe(struct platform_device *pdev)
{
	struct backlight_device *bd;
	struct mxcbl_dev_data *devdata;
	int ret = 0;
 
	devdata = kzalloc(sizeof(struct mxcbl_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;
;
        /* ron: TBD */
	devdata->base = IO_ADDRESS(PWM1_BASE_ADDR);
	devdata->clk = clk_get(&pdev->dev, "pwm");
	devdata->clk_enabled = 0;
	mxc_pwm_enable(devdata);

#if 0 //vv, 20100316 dev.bus_id is deprecated, change to use dev_name(&pdev->dev)
	bd = backlight_device_register(pdev->dev.bus_id, &pdev->dev, devdata,
				       &mxcbl_ops);
#else
	bd = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, devdata,
				       &mxcbl_ops);
#endif

	if (IS_ERR(bd)) {
		ret = PTR_ERR(bd);
		goto err0;
	}

	platform_set_drvdata(pdev, bd);

	bd->props.brightness = MXC_DFL_BL_LEVEL;
	bd->props.max_brightness = MXC_MAX_BL_LEVEL;
	bd->props.power = FB_BLANK_UNBLANK;
	bd->props.fb_blank = FB_BLANK_UNBLANK;
	mxc_set_brightness(devdata, MXC_DFL_BL_LEVEL);

	/*printk("MXC Backlight Device %s Initialized.\n", pdev->dev.bus_id);*/
	printk("MXC Backlight Device %s Initialized.\n", dev_name(&pdev->dev));

	return 0;

err0:

	mxc_pwm_disable(devdata);
	kfree(devdata);
	return ret;
}

static int mxcbl_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
        struct mxcbl_dev_data *devdata = dev_get_drvdata(&bd->dev);

	bd->props.brightness = MXC_INTENSITY_OFF;
	backlight_update_status(bd);
	mxc_pwm_disable(devdata);
	backlight_device_unregister(bd);

	return 0;
}

static int mxcbl_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct mxcbl_dev_data *devdata = dev_get_drvdata(&bd->dev);

	devdata->suspend = 1;
	backlight_update_status(bd);
	mxc_pwm_disable(devdata);

	return 0;
}

static int mxcbl_resume(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct mxcbl_dev_data *devdata = dev_get_drvdata(&bd->dev);

	mxc_pwm_enable(devdata);
	devdata->suspend = 0;
	backlight_update_status(bd);

	return 0;
}

static struct platform_driver mxcbl_driver = {
	.probe = mxcbl_probe,
	.remove = mxcbl_remove,
	.suspend = mxcbl_suspend,
	.resume = mxcbl_resume,
	.driver = {
		   .name = "mxc_efikasb_bl",
		   },
};

static int __init mxcbl_init(void)
{

	return platform_driver_register(&mxcbl_driver);
}

static void __exit mxcbl_exit(void)
{
	platform_driver_unregister(&mxcbl_driver);
}

module_init(mxcbl_init);
module_exit(mxcbl_exit);

MODULE_DESCRIPTION("Genesi Efika MX Smartbook Backlight Driver");
MODULE_AUTHOR("Pegatron Corp.");
MODULE_LICENSE("GPL");
