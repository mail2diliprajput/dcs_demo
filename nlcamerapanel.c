// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017-2018, Bootlin
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

enum nlcamerapanel_op {
    NLCAMERAPANEL_SWITCH_PAGE,
    NLCAMERAPANEL_COMMAND,
};

struct nlcamerapanel_instr {
    enum nlcamerapanel_op	op;

    union arg {
        struct cmd {
            u8	cmd;
            u8	data;
        } cmd;
        u8	page;
    } arg;
};

struct nlcamerapanel_desc {
    const struct nlcamerapanel_instr *init;
    const size_t init_length;
    const struct drm_display_mode *mode;
};

struct nlcamerapanel {
    struct drm_panel	panel;
    struct mipi_dsi_device	*dsi;
    const struct nlcamerapanel_desc	*desc;

    struct regulator	*power;
    struct gpio_desc	*reset;
};

#define NLCAMERAPANEL_SWITCH_PAGE_INSTR(_page)	\
	{					\
		.op = NLCAMERAPANEL_SWITCH_PAGE,	\
		.arg = {			\
			.page = (_page),	\
		},				\
	}

#define NLCAMERAPANEL_COMMAND_INSTR(_cmd, _data)		\
	{						\
		.op = NLCAMERAPANEL_COMMAND,		\
		.arg = {				\
			.cmd = {			\
				.cmd = (_cmd),		\
				.data = (_data),	\
			},				\
		},					\
	}

static const struct nlcamerapanel_instr nlcamerapanel_init[] = {
        NLCAMERAPANEL_SWITCH_PAGE_INSTR(3),
        NLCAMERAPANEL_COMMAND_INSTR(0x01, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x02, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x03, 0x73),
        NLCAMERAPANEL_COMMAND_INSTR(0x04, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x05, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x06, 0x08),
        NLCAMERAPANEL_COMMAND_INSTR(0x07, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x08, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x09, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x0A, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x0B, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x0C, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x0D, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x0E, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x0F, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x10, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x11, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x12, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x13, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x14, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x15, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x16, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x17, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x18, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x19, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x1A, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x1B, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x1C, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x1D, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x1E, 0x40),
        NLCAMERAPANEL_COMMAND_INSTR(0x1F, 0xC0),
        NLCAMERAPANEL_COMMAND_INSTR(0x20, 0x06),
        NLCAMERAPANEL_COMMAND_INSTR(0x21, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x22, 0x06),
        NLCAMERAPANEL_COMMAND_INSTR(0x23, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x24, 0x88),
        NLCAMERAPANEL_COMMAND_INSTR(0x25, 0x88),
        NLCAMERAPANEL_COMMAND_INSTR(0x26, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x27, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x28, 0x3B),
        NLCAMERAPANEL_COMMAND_INSTR(0x29, 0x03),
        NLCAMERAPANEL_COMMAND_INSTR(0x2A, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x2B, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x2C, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x2D, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x2E, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x2F, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x30, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x31, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x32, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x33, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x34, 0x00), /* GPWR1/2 non overlap time 2.62us */
        NLCAMERAPANEL_COMMAND_INSTR(0x35, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x36, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x37, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x38, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x39, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x3A, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x3B, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x3C, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x3D, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x3E, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x3F, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x40, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x41, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x42, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x43, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x44, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x50, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x51, 0x23),
        NLCAMERAPANEL_COMMAND_INSTR(0x52, 0x45),
        NLCAMERAPANEL_COMMAND_INSTR(0x53, 0x67),
        NLCAMERAPANEL_COMMAND_INSTR(0x54, 0x89),
        NLCAMERAPANEL_COMMAND_INSTR(0x55, 0xAB),
        NLCAMERAPANEL_COMMAND_INSTR(0x56, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x57, 0x23),
        NLCAMERAPANEL_COMMAND_INSTR(0x58, 0x45),
        NLCAMERAPANEL_COMMAND_INSTR(0x59, 0x67),
        NLCAMERAPANEL_COMMAND_INSTR(0x5A, 0x89),
        NLCAMERAPANEL_COMMAND_INSTR(0x5B, 0xAB),
        NLCAMERAPANEL_COMMAND_INSTR(0x5C, 0xCD),
        NLCAMERAPANEL_COMMAND_INSTR(0x5D, 0xEF),
        NLCAMERAPANEL_COMMAND_INSTR(0x5E, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x5F, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x60, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x61, 0x06),
        NLCAMERAPANEL_COMMAND_INSTR(0x62, 0x06),
        NLCAMERAPANEL_COMMAND_INSTR(0x63, 0x07),
        NLCAMERAPANEL_COMMAND_INSTR(0x64, 0x07),
        NLCAMERAPANEL_COMMAND_INSTR(0x65, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x66, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x67, 0x02),
        NLCAMERAPANEL_COMMAND_INSTR(0x68, 0x02),
        NLCAMERAPANEL_COMMAND_INSTR(0x69, 0x05),
        NLCAMERAPANEL_COMMAND_INSTR(0x6A, 0x05),
        NLCAMERAPANEL_COMMAND_INSTR(0x6B, 0x02),
        NLCAMERAPANEL_COMMAND_INSTR(0x6C, 0x0D),
        NLCAMERAPANEL_COMMAND_INSTR(0x6D, 0x0D),
        NLCAMERAPANEL_COMMAND_INSTR(0x6E, 0x0C),
        NLCAMERAPANEL_COMMAND_INSTR(0x6F, 0x0C),
        NLCAMERAPANEL_COMMAND_INSTR(0x70, 0x0F),
        NLCAMERAPANEL_COMMAND_INSTR(0x71, 0x0F),
        NLCAMERAPANEL_COMMAND_INSTR(0x72, 0x0E),
        NLCAMERAPANEL_COMMAND_INSTR(0x73, 0x0E),
        NLCAMERAPANEL_COMMAND_INSTR(0x74, 0x02),
        NLCAMERAPANEL_COMMAND_INSTR(0x75, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x76, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x77, 0x06),
        NLCAMERAPANEL_COMMAND_INSTR(0x78, 0x06),
        NLCAMERAPANEL_COMMAND_INSTR(0x79, 0x07),
        NLCAMERAPANEL_COMMAND_INSTR(0x7A, 0x07),
        NLCAMERAPANEL_COMMAND_INSTR(0x7B, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x7C, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0x7D, 0x02),
        NLCAMERAPANEL_COMMAND_INSTR(0x7E, 0x02),
        NLCAMERAPANEL_COMMAND_INSTR(0x7F, 0x05),
        NLCAMERAPANEL_COMMAND_INSTR(0x80, 0x05),
        NLCAMERAPANEL_COMMAND_INSTR(0x81, 0x02),
        NLCAMERAPANEL_COMMAND_INSTR(0x82, 0x0D),
        NLCAMERAPANEL_COMMAND_INSTR(0x83, 0x0D),
        NLCAMERAPANEL_COMMAND_INSTR(0x84, 0x0C),
        NLCAMERAPANEL_COMMAND_INSTR(0x85, 0x0C),
        NLCAMERAPANEL_COMMAND_INSTR(0x86, 0x0F),
        NLCAMERAPANEL_COMMAND_INSTR(0x87, 0x0F),
        NLCAMERAPANEL_COMMAND_INSTR(0x88, 0x0E),
        NLCAMERAPANEL_COMMAND_INSTR(0x89, 0x0E),
        NLCAMERAPANEL_COMMAND_INSTR(0x8A, 0x02),
        NLCAMERAPANEL_SWITCH_PAGE_INSTR(4),
        NLCAMERAPANEL_COMMAND_INSTR(0x3B, 0xC0), /* ILI4003D sel */
        NLCAMERAPANEL_COMMAND_INSTR(0x6C, 0x15), /* Set VCORE voltage = 1.5V */
        NLCAMERAPANEL_COMMAND_INSTR(0x6E, 0x2A), /* di_pwr_reg=0 for power mode 2A, VGH clamp 18V */
        NLCAMERAPANEL_COMMAND_INSTR(0x6F, 0x33), /* pumping ratio VGH=5x VGL=-3x */
        NLCAMERAPANEL_COMMAND_INSTR(0x8D, 0x1B), /* VGL clamp -10V */
        NLCAMERAPANEL_COMMAND_INSTR(0x87, 0xBA), /* ESD */
        NLCAMERAPANEL_COMMAND_INSTR(0x3A, 0x24), /* POWER SAVING */
        NLCAMERAPANEL_COMMAND_INSTR(0x26, 0x76),
        NLCAMERAPANEL_COMMAND_INSTR(0xB2, 0xD1),
        NLCAMERAPANEL_SWITCH_PAGE_INSTR(1),
        NLCAMERAPANEL_COMMAND_INSTR(0x22, 0x0A), /* BGR, SS */
        NLCAMERAPANEL_COMMAND_INSTR(0x31, 0x00), /* Zigzag type3 inversion */
        NLCAMERAPANEL_COMMAND_INSTR(0x40, 0x53), /* ILI4003D sel */
        NLCAMERAPANEL_COMMAND_INSTR(0x43, 0x66),
        NLCAMERAPANEL_COMMAND_INSTR(0x53, 0x4C),
        NLCAMERAPANEL_COMMAND_INSTR(0x50, 0x87),
        NLCAMERAPANEL_COMMAND_INSTR(0x51, 0x82),
        NLCAMERAPANEL_COMMAND_INSTR(0x60, 0x15),
        NLCAMERAPANEL_COMMAND_INSTR(0x61, 0x01),
        NLCAMERAPANEL_COMMAND_INSTR(0x62, 0x0C),
        NLCAMERAPANEL_COMMAND_INSTR(0x63, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0xA0, 0x00),
        NLCAMERAPANEL_COMMAND_INSTR(0xA1, 0x13), /* VP251 */
        NLCAMERAPANEL_COMMAND_INSTR(0xA2, 0x23), /* VP247 */
        NLCAMERAPANEL_COMMAND_INSTR(0xA3, 0x14), /* VP243 */
        NLCAMERAPANEL_COMMAND_INSTR(0xA4, 0x16), /* VP239 */
        NLCAMERAPANEL_COMMAND_INSTR(0xA5, 0x29), /* VP231 */
        NLCAMERAPANEL_COMMAND_INSTR(0xA6, 0x1E), /* VP219 */
        NLCAMERAPANEL_COMMAND_INSTR(0xA7, 0x1D), /* VP203 */
        NLCAMERAPANEL_COMMAND_INSTR(0xA8, 0x86), /* VP175 */
        NLCAMERAPANEL_COMMAND_INSTR(0xA9, 0x1E), /* VP144 */
        NLCAMERAPANEL_COMMAND_INSTR(0xAA, 0x29), /* VP111 */
        NLCAMERAPANEL_COMMAND_INSTR(0xAB, 0x74), /* VP80 */
        NLCAMERAPANEL_COMMAND_INSTR(0xAC, 0x19), /* VP52 */
        NLCAMERAPANEL_COMMAND_INSTR(0xAD, 0x17), /* VP36 */
        NLCAMERAPANEL_COMMAND_INSTR(0xAE, 0x4B), /* VP24 */
        NLCAMERAPANEL_COMMAND_INSTR(0xAF, 0x20), /* VP16 */
        NLCAMERAPANEL_COMMAND_INSTR(0xB0, 0x26), /* VP12 */
        NLCAMERAPANEL_COMMAND_INSTR(0xB1, 0x4C), /* VP8 */
        NLCAMERAPANEL_COMMAND_INSTR(0xB2, 0x5D), /* VP4 */
        NLCAMERAPANEL_COMMAND_INSTR(0xB3, 0x3F), /* VP0 */
        NLCAMERAPANEL_COMMAND_INSTR(0xC0, 0x00), /* VN255 GAMMA N */
        NLCAMERAPANEL_COMMAND_INSTR(0xC1, 0x13), /* VN251 */
        NLCAMERAPANEL_COMMAND_INSTR(0xC2, 0x23), /* VN247 */
        NLCAMERAPANEL_COMMAND_INSTR(0xC3, 0x14), /* VN243 */
        NLCAMERAPANEL_COMMAND_INSTR(0xC4, 0x16), /* VN239 */
        NLCAMERAPANEL_COMMAND_INSTR(0xC5, 0x29), /* VN231 */
        NLCAMERAPANEL_COMMAND_INSTR(0xC6, 0x1E), /* VN219 */
        NLCAMERAPANEL_COMMAND_INSTR(0xC7, 0x1D), /* VN203 */
        NLCAMERAPANEL_COMMAND_INSTR(0xC8, 0x86), /* VN175 */
        NLCAMERAPANEL_COMMAND_INSTR(0xC9, 0x1E), /* VN144 */
        NLCAMERAPANEL_COMMAND_INSTR(0xCA, 0x29), /* VN111 */
        NLCAMERAPANEL_COMMAND_INSTR(0xCB, 0x74), /* VN80 */
        NLCAMERAPANEL_COMMAND_INSTR(0xCC, 0x19), /* VN52 */
        NLCAMERAPANEL_COMMAND_INSTR(0xCD, 0x17), /* VN36 */
        NLCAMERAPANEL_COMMAND_INSTR(0xCE, 0x4B), /* VN24 */
        NLCAMERAPANEL_COMMAND_INSTR(0xCF, 0x20), /* VN16 */
        NLCAMERAPANEL_COMMAND_INSTR(0xD0, 0x26), /* VN12 */
        NLCAMERAPANEL_COMMAND_INSTR(0xD1, 0x4C), /* VN8 */
        NLCAMERAPANEL_COMMAND_INSTR(0xD2, 0x5D), /* VN4 */
        NLCAMERAPANEL_COMMAND_INSTR(0xD3, 0x3F), /* VN0 */
};

static inline struct nlcamerapanel *panel_to_nlcamerapanel(struct drm_panel *panel)
{
    return container_of(panel, struct nlcamerapanel, panel);
}

/*
 * The panel seems to accept some private DCS commands that map
 * directly to registers.
 *
 * It is organised by page, with each page having its own set of
 * registers, and the first page looks like it's holding the standard
 * DCS commands.
 *
 * So before any attempt at sending a command or data, we have to be
 * sure if we're in the right page or not.
 */
static int nlcamerapanel_switch_page(struct nlcamerapanel *ctx, u8 page)
{
    u8 buf[4] = { 0xff, 0x98, 0x81, page };
    int ret;

    ret = mipi_dsi_dcs_write_buffer(ctx->dsi, buf, sizeof(buf));
    if (ret < 0)
        return ret;

    return 0;
}

static int nlcamerapanel_send_cmd_data(struct nlcamerapanel *ctx, u8 cmd, u8 data)
{
    u8 buf[2] = { cmd, data };
    int ret;

    ret = mipi_dsi_dcs_write_buffer(ctx->dsi, buf, sizeof(buf));
    if (ret < 0)
        return ret;

    return 0;
}

static int nlcamerapanel_prepare(struct drm_panel *panel)
{
    printk(KERN_ERR "nlcamerapanel_prepare");
    struct nlcamerapanel *ctx = panel_to_nlcamerapanel(panel);
    unsigned int i;
    int ret;

    /* Power the panel */
    printk(KERN_ERR "nlcamerapanel regulator_enable");
    ret = regulator_enable(ctx->power);
    if (ret)
        return ret;
    msleep(5);

    /* And reset it */
    printk(KERN_ERR "nlcamerapanel reset 1");
    gpiod_set_value(ctx->reset, 1);
    msleep(150);

    printk(KERN_ERR "nlcamerapanel reset 0");
    gpiod_set_value(ctx->reset, 0);
    msleep(400);

    printk(KERN_ERR "nlcamerapanel init sequence");
    for (i = 0; i < ctx->desc->init_length; i++) {
        const struct nlcamerapanel_instr *instr = &ctx->desc->init[i];

        if (instr->op == NLCAMERAPANEL_SWITCH_PAGE)
            ret = nlcamerapanel_switch_page(ctx, instr->arg.page);
        else if (instr->op == NLCAMERAPANEL_COMMAND)
            ret = nlcamerapanel_send_cmd_data(ctx, instr->arg.cmd.cmd,
                                         instr->arg.cmd.data);

        if (ret)
            return ret;
    }

    ret = nlcamerapanel_switch_page(ctx, 0);
    if (ret)
        return ret;

    printk(KERN_ERR "nlcamerapanel set tear on");
    ret = mipi_dsi_dcs_set_tear_on(ctx->dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
    if (ret)
        return ret;

    printk(KERN_ERR "nlcamerapanel exit sleep mode");
    ret = mipi_dsi_dcs_exit_sleep_mode(ctx->dsi);
    if (ret)
        return ret;

    return 0;
}

static int nlcamerapanel_enable(struct drm_panel *panel)
{
    struct nlcamerapanel *ctx = panel_to_nlcamerapanel(panel);

    msleep(120);

    mipi_dsi_dcs_set_display_on(ctx->dsi);

    return 0;
}

static int nlcamerapanel_disable(struct drm_panel *panel)
{
    struct nlcamerapanel *ctx = panel_to_nlcamerapanel(panel);

    return mipi_dsi_dcs_set_display_off(ctx->dsi);
}

static int nlcamerapanel_unprepare(struct drm_panel *panel)
{
    struct nlcamerapanel *ctx = panel_to_nlcamerapanel(panel);

    mipi_dsi_dcs_enter_sleep_mode(ctx->dsi);
    regulator_disable(ctx->power);
    gpiod_set_value(ctx->reset, 1);

    return 0;
}

static const struct drm_display_mode nlcamerapanel_default_mode = {
        .clock		= 69700,

        .hdisplay	= 800,
        .hsync_start	= 800 + 52,
        .hsync_end	= 800 + 52 + 8,
        .htotal		= 800 + 52 + 8 + 48,

        .vdisplay	= 1280,
        .vsync_start	= 1280 + 16,
        .vsync_end	= 1280 + 16 + 6,
        .vtotal		= 1280 + 16 + 6 + 15,

        .width_mm	= 135,
        .height_mm	= 217,
};

static int nlcamerapanel_get_modes(struct drm_panel *panel,
                              struct drm_connector *connector)
{
    struct nlcamerapanel *ctx = panel_to_nlcamerapanel(panel);
    struct drm_display_mode *mode;

    mode = drm_mode_duplicate(connector->dev, ctx->desc->mode);
    if (!mode) {
        dev_err(&ctx->dsi->dev, "failed to add mode %ux%ux@%u\n",
                ctx->desc->mode->hdisplay,
                ctx->desc->mode->vdisplay,
                drm_mode_vrefresh(ctx->desc->mode));
        return -ENOMEM;
    }

    drm_mode_set_name(mode);

    mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
    drm_mode_probed_add(connector, mode);

    connector->display_info.width_mm = mode->width_mm;
    connector->display_info.height_mm = mode->height_mm;

    return 1;
}

static const struct drm_panel_funcs nlcamerapanel_funcs = {
        .prepare	= nlcamerapanel_prepare,
        .unprepare	= nlcamerapanel_unprepare,
        .enable		= nlcamerapanel_enable,
        .disable	= nlcamerapanel_disable,
        .get_modes	= nlcamerapanel_get_modes,
};

static int nlcamerapanel_dsi_probe(struct mipi_dsi_device *dsi)
{
    struct nlcamerapanel *ctx;
    int ret;

    printk(KERN_ERR "nlcamerapanel_dsi_probe called\n");

    ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;
    mipi_dsi_set_drvdata(dsi, ctx);
    ctx->dsi = dsi;
    ctx->desc = of_device_get_match_data(&dsi->dev);

    printk(KERN_ERR "nlcamerapanel drm_panel_init\n");
    drm_panel_init(&ctx->panel, &dsi->dev, &nlcamerapanel_funcs,
                   DRM_MODE_CONNECTOR_DSI);

    ctx->power = devm_regulator_get(&dsi->dev, "power");
    if (IS_ERR(ctx->power)) {
        dev_err(&dsi->dev, "Couldn't get our power regulator\n");
        return PTR_ERR(ctx->power);
    }

    ctx->reset = devm_gpiod_get(&dsi->dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(ctx->reset)) {
        dev_err(&dsi->dev, "Couldn't get our reset GPIO\n");
        return PTR_ERR(ctx->reset);
    }

    ret = drm_panel_of_backlight(&ctx->panel);
    if (ret)
        return ret;

    printk(KERN_ERR "nlcamerapanel drm_panel_add\n");
    drm_panel_add(&ctx->panel);

    dsi->mode_flags = MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_VIDEO;
    dsi->format = MIPI_DSI_FMT_RGB888;
    dsi->lanes = 4;

    printk(KERN_ERR "nlcamerapanel mipi_dsi_attach\n");
    return mipi_dsi_attach(dsi);
}

static int nlcamerapanel_dsi_remove(struct mipi_dsi_device *dsi)
{
    struct nlcamerapanel *ctx = mipi_dsi_get_drvdata(dsi);

    mipi_dsi_detach(dsi);
    drm_panel_remove(&ctx->panel);

    return 0;
}

static const struct nlcamerapanel_desc nlcamerapanel_desc = {
        .init = nlcamerapanel_init,
        .init_length = ARRAY_SIZE(nlcamerapanel_init),
        .mode = &nlcamerapanel_default_mode,
};

static const struct of_device_id nlcamerapanel_of_match[] = {
        { .compatible = "nlacoustics,nlcamerapanel", .data = &nlcamerapanel_desc },
        { }
};
MODULE_DEVICE_TABLE(of, nlcamerapanel_of_match);

static struct mipi_dsi_driver nlcamerapanel_dsi_driver = {
        .probe		= nlcamerapanel_dsi_probe,
        .remove		= nlcamerapanel_dsi_remove,
        .driver = {
                .name		= "nlcamerapanel",
                .of_match_table	= nlcamerapanel_of_match,
        },
};
module_mipi_dsi_driver(nlcamerapanel_dsi_driver);

MODULE_AUTHOR("Ilmo Euro <ilmo.euro@nlacoustics.com>");
MODULE_DESCRIPTION("NL Camera display panel controller driver");
MODULE_LICENSE("GPL v2");