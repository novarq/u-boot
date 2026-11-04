// SPDX-License-Identifier: GPL-2.0+
/* Microchip LAN969x serial GPIO controller */

#include <asm/gpio.h>
#include <asm/io.h>
#include <clk.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/libfdt.h>

#define SGPIO_PORTS		32
#define SGPIO_BITS_PER_PORT	4

#define SGPIO_INPUT_DATA(bit)	(0x00 + (bit) * 4)
#define SGPIO_SIO_CONFIG	0x10
#define SGPIO_SIO_CLOCK		0x14
#define SGPIO_PORT_CONFIG(port)	(0x18 + (port) * 4)
#define SGPIO_PORT_ENABLE	0x98

#define SGPIO_AUTO_REPEAT	BIT(6)
#define SGPIO_SINGLE_SHOT	BIT(7)
#define SGPIO_PORT_WIDTH	GENMASK(4, 3)
#define SGPIO_CLOCK_DIV		GENMASK(19, 8)
#define SGPIO_SOURCE_SHIFT(bit)	(12 + 3 * (bit))

struct microchip_sgpio_priv {
	void __iomem *regs;
	u32 ports;
	u32 bits_per_port;
};

static int microchip_sgpio_port_bit(struct udevice *dev, unsigned int offset,
				   u32 *port, u32 *bit)
{
	struct microchip_sgpio_priv *priv = dev_get_priv(dev_get_parent(dev));

	*port = offset / priv->bits_per_port;
	*bit = offset % priv->bits_per_port;
	if (*port >= SGPIO_PORTS || !(priv->ports & BIT(*port)))
		return -EINVAL;

	return 0;
}

static int microchip_sgpio_xlate(struct udevice *dev, struct gpio_desc *desc,
				 struct ofnode_phandle_args *args)
{
	struct microchip_sgpio_priv *priv = dev_get_priv(dev_get_parent(dev));
	u32 port, bit;

	if (args->args_count != 3)
		return -EINVAL;

	port = args->args[0];
	bit = args->args[1];
	if (port >= SGPIO_PORTS || bit >= priv->bits_per_port ||
	    !(priv->ports & BIT(port)))
		return -EINVAL;

	desc->offset = port * priv->bits_per_port + bit;
	desc->flags = gpio_flags_xlate(args->args[2]);

	return 0;
}

static int microchip_sgpio_request(struct udevice *dev, unsigned int offset,
				  const char *label)
{
	u32 port, bit;

	return microchip_sgpio_port_bit(dev, offset, &port, &bit);
}

static int microchip_sgpio_set_output(struct udevice *dev,
				     unsigned int offset, int value)
{
	struct microchip_sgpio_priv *priv = dev_get_priv(dev_get_parent(dev));
	void __iomem *config = priv->regs + SGPIO_SIO_CONFIG;
	u32 port, bit, mask, old, val;
	int ret;

	if (dev_read_u32_default(dev, "reg", 0) != 1)
		return -EINVAL;
	ret = microchip_sgpio_port_bit(dev, offset, &port, &bit);
	if (ret)
		return ret;

	mask = BIT(SGPIO_SOURCE_SHIFT(bit));
	old = readl(priv->regs + SGPIO_PORT_CONFIG(port));
	val = (old & ~mask) | (value ? mask : 0);
	if (val == old)
		return 0;
	writel(val, priv->regs + SGPIO_PORT_CONFIG(port));

	/* Push the new value immediately rather than waiting for the next scan. */
	val = readl(config);
	writel((val & ~SGPIO_AUTO_REPEAT) | SGPIO_SINGLE_SHOT, config);
	ret = readl_poll_timeout(config, old, !(old & SGPIO_SINGLE_SHOT),
				 60000);
	writel(readl(config) | SGPIO_AUTO_REPEAT, config);

	return ret;
}

static int microchip_sgpio_direction_input(struct udevice *dev,
					  unsigned int offset)
{
	return dev_read_u32_default(dev, "reg", 0) == 0 ? 0 : -EINVAL;
}

static int microchip_sgpio_direction_output(struct udevice *dev,
					   unsigned int offset, int value)
{
	return microchip_sgpio_set_output(dev, offset, value);
}

static int microchip_sgpio_get_value(struct udevice *dev, unsigned int offset)
{
	struct microchip_sgpio_priv *priv = dev_get_priv(dev_get_parent(dev));
	u32 port, bit, val;
	int ret;

	ret = microchip_sgpio_port_bit(dev, offset, &port, &bit);
	if (ret)
		return ret;

	if (dev_read_u32_default(dev, "reg", 0) == 0)
		val = readl(priv->regs + SGPIO_INPUT_DATA(bit)) & BIT(port);
	else
		val = readl(priv->regs + SGPIO_PORT_CONFIG(port)) &
		      BIT(SGPIO_SOURCE_SHIFT(bit));

	return !!val;
}

static int microchip_sgpio_set_value(struct udevice *dev, unsigned int offset,
				    int value)
{
	return microchip_sgpio_set_output(dev, offset, value);
}

static int microchip_sgpio_get_function(struct udevice *dev,
				       unsigned int offset)
{
	return dev_read_u32_default(dev, "reg", 0) == 0 ?
		GPIOF_INPUT : GPIOF_OUTPUT;
}

static const struct dm_gpio_ops microchip_sgpio_ops = {
	.request = microchip_sgpio_request,
	.xlate = microchip_sgpio_xlate,
	.direction_input = microchip_sgpio_direction_input,
	.direction_output = microchip_sgpio_direction_output,
	.get_value = microchip_sgpio_get_value,
	.set_value = microchip_sgpio_set_value,
	.get_function = microchip_sgpio_get_function,
};

static int microchip_sgpio_bank_probe(struct udevice *dev)
{
	struct microchip_sgpio_priv *priv = dev_get_priv(dev_get_parent(dev));
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	u32 bank = dev_read_u32_default(dev, "reg", 2);
	u32 count = dev_read_u32_default(dev, "ngpios", 0);

	if (bank > 1 || !count || count > SGPIO_PORTS * priv->bits_per_port)
		return -EINVAL;

	uc_priv->gpio_count = count;
	uc_priv->bank_name = bank ? "sgpio-out" : "sgpio-in";

	return 0;
}

static int microchip_sgpio_probe(struct udevice *dev)
{
	struct microchip_sgpio_priv *priv = dev_get_priv(dev);
	const fdt32_t *ranges;
	struct clk clk;
	ulong rate;
	u32 freq, count, config, div;
	int len, i, port, ret;

	priv->regs = dev_read_addr_ptr(dev);
	if (!priv->regs)
		return -EINVAL;

	ranges = dev_read_prop(dev, "microchip,sgpio-port-ranges", &len);
	if (!ranges || len < 2 * sizeof(*ranges) ||
	    len % (2 * sizeof(*ranges)))
		return -EINVAL;
	for (i = 0; i < len / sizeof(*ranges); i += 2) {
		u32 first = fdt32_to_cpu(ranges[i]);
		u32 last = fdt32_to_cpu(ranges[i + 1]);

		if (first > last || last >= SGPIO_PORTS)
			return -EINVAL;
		priv->ports |= GENMASK(last, first);
	}

	count = dev_read_u32_default(dev, "ngpios", 128);
	priv->bits_per_port = DIV_ROUND_UP(count, SGPIO_PORTS);
	if (!priv->bits_per_port || priv->bits_per_port > SGPIO_BITS_PER_PORT)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret)
		return ret;
	rate = clk_get_rate(&clk);
	if (IS_ERR_VALUE(rate))
		return -EINVAL;
	freq = dev_read_u32_default(dev, "bus-frequency", 12500000);
	if (!freq || freq > rate / 2)
		return -EINVAL;
	div = max_t(u32, 2, rate / freq);
	if (div > 0xfff)
		return -EINVAL;

	config = readl(priv->regs + SGPIO_SIO_CONFIG);
	config &= ~SGPIO_PORT_WIDTH;
	config |= (priv->bits_per_port - 1) << 3;
	config |= SGPIO_AUTO_REPEAT;
	writel(config, priv->regs + SGPIO_SIO_CONFIG);
	config = readl(priv->regs + SGPIO_SIO_CLOCK);
	config &= ~SGPIO_CLOCK_DIV;
	config |= div << 8;
	writel(config, priv->regs + SGPIO_SIO_CLOCK);

	for (port = 0; port < SGPIO_PORTS; port++)
		writel(0, priv->regs + SGPIO_PORT_CONFIG(port));
	writel(priv->ports, priv->regs + SGPIO_PORT_ENABLE);

	return 0;
}

static const struct udevice_id microchip_sgpio_ids[] = {
	{ .compatible = "microchip,lan9691-sgpio" },
	{ .compatible = "microchip,sparx5-sgpio" },
	{ }
};

static const struct udevice_id microchip_sgpio_bank_ids[] = {
	{ .compatible = "microchip,lan9691-sgpio-bank" },
	{ .compatible = "microchip,sparx5-sgpio-bank" },
	{ }
};

U_BOOT_DRIVER(microchip_sgpio) = {
	.name = "microchip_sgpio",
	.id = UCLASS_MISC,
	.of_match = microchip_sgpio_ids,
	.probe = microchip_sgpio_probe,
	.priv_auto = sizeof(struct microchip_sgpio_priv),
};

U_BOOT_DRIVER(microchip_sgpio_bank) = {
	.name = "microchip_sgpio_bank",
	.id = UCLASS_GPIO,
	.of_match = microchip_sgpio_bank_ids,
	.ops = &microchip_sgpio_ops,
	.probe = microchip_sgpio_bank_probe,
};
