/*
 * linux/drivers/video/backlight/pwm_bl.c
 *
 * simple PWM based backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>

struct pwm_bl_data {
	struct pwm_device	*pwm;
	unsigned int		period;
	bool			power_state;
	int			brightness;
	int			max_brightness;
	struct notifier_block	notifier;
	int			(*notify)(int brightness);
	void			(*power)(int state);
};

static int pwm_backlight_update_status(struct backlight_device *bl)
{
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
	int brightness, max;

	brightness = pb->brightness = bl->props.brightness;
	max = pb->max_brightness = bl->props.max_brightness;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(brightness);

	if (brightness == 0) {
		if (pb->power) {
			pb->power(0);
			pb->power_state = 0;
		}
		pwm_config(pb->pwm, 0, pb->period);
		pwm_disable(pb->pwm);
	} else {
		pwm_config(pb->pwm, brightness * pb->period / max, pb->period);
		pwm_enable(pb->pwm);
		if (pb->power && !pb->power_state) {
			pb->power(1);
			pb->power_state = 1;
		}
	}
	return 0;
}

static int pwm_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static void pwm_bl_blank(struct pwm_bl_data *pb, int type)
{
	switch (type) {
		case FB_BLANK_UNBLANK:
			pwm_config(pb->pwm, pb->brightness * pb->period / pb->max_brightness, pb->period);
			pwm_enable(pb->pwm);
			if (pb->power && !pb->power_state) {
				pb->power(1);
				pb->power_state = 1;
			}
		break;
		case FB_BLANK_NORMAL:
		case FB_BLANK_POWERDOWN:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
			if (pb->power) {
				pb->power(0);
				pb->power_state = 0;
			}
			pwm_config(pb->pwm, 0, pb->period);
			pwm_disable(pb->pwm);
		break;
	}
}

static int pwm_backlight_check_fb(struct fb_info *info,int blanktype)
{
	char *id = info->fix.id;
	if (!strncmp(id, "DISP3 BG", 8))
	    return 1;
	else {
        if(!strcmp(id,"DISP3 FG") && blanktype == FB_BLANK_UNBLANK)
            return 1;
    }
	return 0;
}

static int pwm_bl_notifier_call(struct notifier_block *p,
				unsigned long event, void *data)
{
	struct pwm_bl_data *pb = container_of(p,
					struct pwm_bl_data, notifier);
	struct fb_event *fb_event = data;
	int *blank = fb_event->data;

	if (!pwm_backlight_check_fb(fb_event->info,*blank))
		return 0;

	switch (event) {
	case FB_EVENT_BLANK:
		pwm_bl_blank(pb, *blank);
		break;
	case FB_EVENT_FB_REGISTERED:
		pwm_bl_blank(pb, FB_BLANK_UNBLANK);
		break;
	case FB_EVENT_FB_UNREGISTERED:
		pwm_bl_blank(pb, FB_BLANK_NORMAL);
		break;
	}

	return 0;
}

static struct backlight_ops pwm_backlight_ops = {
	.update_status	= pwm_backlight_update_status,
	.get_brightness	= pwm_backlight_get_brightness,
	.check_fb = pwm_backlight_check_fb,
};

static int pwm_backlight_probe(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct pwm_bl_data *pb;
	int ret;

	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	if (data->init) {
		ret = data->init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	pb = kzalloc(sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	pb->period = data->pwm_period_ns;
	pb->notify = data->notify;
	pb->power = data->power;

	pb->pwm = pwm_request(data->pwm_id, "backlight");
	if (IS_ERR(pb->pwm)) {
		dev_err(&pdev->dev, "unable to request PWM for backlight\n");
		ret = PTR_ERR(pb->pwm);
		goto err_pwm;
	} else
		dev_dbg(&pdev->dev, "got pwm for backlight\n");

	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev,
			pb, &pwm_backlight_ops);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

	pb->notifier.notifier_call = pwm_bl_notifier_call;

	ret = fb_register_client(&pb->notifier);

	bl->props.max_brightness = data->max_brightness;
	bl->props.brightness = data->dft_brightness;
	backlight_update_status(bl);

	platform_set_drvdata(pdev, bl);
	return 0;

err_bl:
	pwm_free(pb->pwm);
err_pwm:
	kfree(pb);
err_alloc:
	if (data->exit)
		data->exit(&pdev->dev);
	return ret;
}

static int pwm_backlight_remove(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	fb_unregister_client(&pb->notifier);

	backlight_device_unregister(bl);
	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);
	pwm_free(pb->pwm);
	kfree(pb);
	if (data->exit)
		data->exit(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM
static int pwm_backlight_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	if (pb->notify)
		pb->notify(0);
	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);
	return 0;
}

static int pwm_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	backlight_update_status(bl);
	return 0;
}
#else
#define pwm_backlight_suspend	NULL
#define pwm_backlight_resume	NULL
#endif

static struct platform_driver pwm_backlight_driver = {
	.driver		= {
		.name	= "pwm-backlight",
		.owner	= THIS_MODULE,
	},
	.probe		= pwm_backlight_probe,
	.remove		= pwm_backlight_remove,
	.suspend	= pwm_backlight_suspend,
	.resume		= pwm_backlight_resume,
};

static int __init pwm_backlight_init(void)
{
	return platform_driver_register(&pwm_backlight_driver);
}
module_init(pwm_backlight_init);

static void __exit pwm_backlight_exit(void)
{
	platform_driver_unregister(&pwm_backlight_driver);
}
module_exit(pwm_backlight_exit);

MODULE_DESCRIPTION("PWM based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-backlight");

