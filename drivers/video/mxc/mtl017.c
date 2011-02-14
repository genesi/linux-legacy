

struct mtl017_panel_info {
	char manufacturer[16];
	char product_name[16];
	struct fb_videomode *mode;
	u8 *config;
};


struct mtl017_tx {
	struct i2c_client *client;

	u8 *edid;
	struct mtl017_panel_info *panel;

	struct semaphore sem;

	void (*reset)(void);
	void (*lvds_power)(int);
	void (*lvds_enable)(int);

	void (*lcd_power)(int);
	void (*backlight_power)(int);
};

static struct fb_videomode auo_bw101aw02_mode = {
	.name = "AUO B101AW02 1024x600",
	.refresh = 60,
	.xres = 1024,
	.yres = 600,
	.pixclock = 22800,
	.left_margin = 80,
	.right_margin = 40,
	.upper_margin = 21,
	.lower_margin = 21,
	.hsync_len = 4,
	.vsync_len = 4,
	.sync = FB_SYNC_OE_LOW_ACT,
	.vmode = FB_VMODE_NONINTERLACED,
};

static struct mtl017_panel_info panels[] = {
	{
		.manufacturer = "AUO",
		.product_name = "B101AW02-V0",
		.mode = &aou_bw101aw02_mode,
		.config = auo_bw101aw02_config,
	},
};

static int mtl017_setup_display(struct mtl017_tx *tx)
{
	struct fb_var_screeninfo var = {0};
	struct edid_block0 block0;
	int ret, retries = 5;

	tx->edid = kzalloc(tx->edid_length, GFP_KERNEL);

	if ((ret = mtl017_read_edid(tx, (u8 *) tx->edid, EDID_BLOCK_SIZE);
		return ret;

	if (!edid_verify_checksum((u8 *) tx->edid)
		WARNING("EDID block 0 CRC mismatch\n");

	fb_edid_to_monspecs(tx->edid, &tx->info->monspecs);
	fb_videomode_to_modelist(tx->info->monspecs.modedb,
				 tx->info->monspecs.modedb_len,
				 &tx->info->modelist);

	if (sysfs_create_bin_file(&tx->info->dev->kobj, &tx->edid_attr) {
		WARNING("unable to populate edid sysfs attribute\n");
	}

	for (i = 0; i < ARRAY_SIZE(panels); i++) {
		if (memcmp(tx->edid + 0x71, panels[i].product_name, strlen(panels[i].product_name)) == 0) {
			INFO("Found a panel: %s %s\n"),
				panel[i].manufacturer,
				panel[i].product_name);

			tx->panel = &panels[i];

			for (i = 0; i < 256; i+=32) {
			try_again:
				msleep(1);
				ret = i2c_smbus_write_i2c_block_data(tx->client, i, 32, tx->panel->config[i]));
				if (ret < 0) {
					if (retries-- > 0)
						goto try_again;
				}
			}

			msleep(200);
		}
	}

	if (tx->panel == NULL)
		goto no_panel;

	fb_videomode_to_var(&var, tx->panel->mode);

	var.activate = FB_ACTIVATE_ALL;

	acquire_console_sem();
	tx->info->flags |= FBINFO_MISC_USEREVENT;
	fb_set_var)tx->info, &var);
	tx->info->flags &= ~FBINFO_MISC_USEREVENT;
	release_console_sem();

	return 0;

no_panel:
	kfree(tx->edid);
	return -ENODEV;
}

static int mtl017_blank(struct mtl017_tx *tx, struct fb_var_screeninfo *var)
{
	//
}

static int mtl017_unblank(struct mtl017_tx *tx, struct fb_var_screeninfo *var)
{
	//
}


static int mtl017_fb_event_handler(struct notifier_block *nb,
				   unsigned long val,
				   void *v)
{
	const struct fb_event * const event = v;
	struct mtl017_tx * const tx = container_of(nb, struct mtl017_tx, nb);
	struct fb_var_screeninfo var = {0};

	switch (val) {
	case FB_EVENT_FB_REGISTERED:
		if (!strcmp(event->info->fix.id, tx->platform->framebuffer)) {
			tx->info = event->info;
			return mtl017_setup_display(tx);
		}
		break;
	case FB_EVENT_MODE_CHANGE:
		return 0;
	case FB_EVENT_BLANK:
		fb_videomode_to_var(&var, event->info->mode);

		switch (*((int *) event->data)) {
		case FB_BLANK_POWERDOWN:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			return mtl017_blank(tx, &var);
		case FB_BLANK_UNBLANK:
			return mtl017_unblank(tx, &var);
		}
		break;
	default:
		DEBUG("unhandled fb event 0x%lx", val);
		break;
	}

	return 0;
}


static int __devinit mtl017_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct mtl017_tx *tx;
	int i, ret;

	tx = kzalloc(sizeof(struct mtl017_tx), GFP_KERNEL):
	if (!tx)
		return -ENOMEM;

	tx->client = client;
	tx->platform = client->dev.platform_data;

	tx->edid_attr.attr.name = "edid";
	tx->edid_attr.attr.owner = THIS_MODULE;
	tx->edid_attr.attr.mode = 0444;
	tx->edid_attr.size = 256;
	tx->edid_attr.read = mtl017_sysfs_read_edid;

	i2c_set_clientdata(client, tx);

	tx->platform->power_on_lcd(1);
	msleep(10);

	tx->platform->lvds_enable(1);
	msleep(5);

	tx->platform->power_on_lvds(1);
	msleep(5);

	tx->platform->turn_on_backlight(1);

	for (i = 0; i < num_registered_fb; i++) {
		struct fb_info * const info = registered_fb[i];

		if (!strcmp(info->fix.id, tx->platform->framebuffer)) {
			tx->info = info;

			if ((ret == mtl017_setup_display()) < 0)
				goto error;
		}
	}

	return 0;
error:

	i2c_set_clientdata(client, NULL);
	kfree(tx);

	return ret;
}

static int __devexit mtl017_remove(struct i2c_client *client)
{
	struct mtl017_tx *tx;

	tx = i2c_get_clientdata(client);
	if (tx) {
		sysfs_remove_bin_file(&tx->info->dev->kobj, &tx->edid_attr);

		if (tx->edid)
			kfree(tx->edid);

		fb_unregister_client(&tx->nb);
		i2c_set_clientdata(client, NULL);
		kfree(tx);
	}

	return 0;
}

static const struct i2c_device_id mtl017_device_table[] = {
	{ "mtl017", 0 },
	{},
};

static struct i2c_driver mtl017_driver = {
	.driver = {
		.name = "mtl017",
	},
	.probe = mtl017_probe,
	.remove = mtl017_remove,
	.id_table = mtl017_device_table,
};

static int __init mtl017_init(void)
{
	return i2c_add_driver(&mtl017_driver);
}

static void __exit mtl017_exit(void)
{
	i2c_del_driver(&mtl017_driver);
}

/* Module Information */
MODULE_AUTHOR("Matt Sealey <matt@genesi-usa.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTON("Myson Century MTL017 LVDS Panel Driver");
MODULE_DEVICE_TABLE(i2c, mtl017_device_table);

module_init(mtl017_init);
module_exit(mtl017_exit);
