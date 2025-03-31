// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2018 Microsemi Corporation
 */

#include <common.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <miiphy.h>
#include <phy.h>
#include <asm/io.h>
#include <wait_bit.h>
#include <linux/bitops.h>
#include <linux/printk.h>

#define MIIM_STATUS			0x0
#define		MIIM_STAT_BUSY			BIT(3)
#define MIIM_CMD			0x8
#define		MIIM_CMD_SCAN		BIT(0)
#define		MIIM_CMD_OPR_WRITE	BIT(1)
#define		MIIM_CMD_OPR_READ	BIT(2)
#define		MIIM_CMD_SINGLE_SCAN	BIT(3)
#define		MIIM_CMD_WRDATA(x)	((x) << 4)
#define		MIIM_CMD_REGAD(x)	((x) << 20)
#define		MIIM_CMD_PHYAD(x)	((x) << 25)
#define		MIIM_CMD_VLD		BIT(31)
#define MIIM_DATA			0xC
#define		MIIM_DATA_ERROR		(0x2 << 16)

struct mscc_mdio_priv {
	void *regs;
};

static int mscc_mdio_wait_ready(struct mscc_mdio_priv *priv)
{
	return wait_for_bit_le32(priv->regs + MIIM_STATUS, MIIM_STAT_BUSY,
				 false, 250, false);
}

static int mscc_mdio_read(struct udevice *dev, int addr, int devad, int reg)
{
	struct mscc_mdio_priv *priv = dev_get_priv(dev);
	u32 val;
	int ret;

	ret = mscc_mdio_wait_ready(priv);
	if (ret)
		goto out;

	writel(MIIM_CMD_VLD | MIIM_CMD_PHYAD(addr) |
	       MIIM_CMD_REGAD(reg) | MIIM_CMD_OPR_READ,
	       priv->regs + MIIM_CMD);

	ret = mscc_mdio_wait_ready(priv);
	if (ret)
		goto out;

	val = readl(priv->regs + MIIM_DATA);
	if (val & MIIM_DATA_ERROR) {
		ret = -EIO;
		goto out;
	}

	ret = val & 0xFFFF;
 out:
	return ret;
}

static int mscc_mdio_write(struct udevice *dev, int addr, int devad, int reg,
			   u16 value)
{
	struct mscc_mdio_priv *priv = dev_get_priv(dev);
	int ret;

	ret = mscc_mdio_wait_ready(priv);
	if (ret < 0)
		goto out;

	writel(MIIM_CMD_VLD | MIIM_CMD_PHYAD(addr) |
	       MIIM_CMD_REGAD(reg) | MIIM_CMD_WRDATA(value) |
	       MIIM_CMD_OPR_WRITE, priv->regs + MIIM_CMD);
 out:
	return ret;
}

/* Get device base address and type, either C22 SMII or C45 XSMI */
static int mscc_mdio_probe(struct udevice *dev)
{
	struct mscc_mdio_priv *priv = dev_get_priv(dev);

	priv->regs = dev_read_addr_ptr(dev);

	return 0;
}

static const struct mdio_ops mscc_mdio_ops = {
	.read = mscc_mdio_read,
	.write = mscc_mdio_write,
};

static const struct udevice_id mscc_mdio_ids[] = {
	{ .compatible = "mscc,ocelot-miim" },
	{ }
};

U_BOOT_DRIVER(mscc_mdio) = {
	.name			= "mscc_mdio",
	.id			= UCLASS_MDIO,
	.of_match		= mscc_mdio_ids,
	.probe			= mscc_mdio_probe,
	.ops			= &mscc_mdio_ops,
	.priv_auto		= sizeof(struct mscc_mdio_priv),
};
