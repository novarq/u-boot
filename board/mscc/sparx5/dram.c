// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2022 Microsemi Corporation
 */

#include <common.h>

#include <ddr_init.h>
#include <ddr_reg.h>

DECLARE_GLOBAL_DATA_PTR;

static uint32_t fdt_read_uint32(const void *fdt, int nodeoffset, const char *prop)
{
	const fdt32_t *ac;
	int len;

	ac = fdt_getprop(fdt, nodeoffset, prop, &len);
	if (!ac)
		panic("%s: no %s property\n", __func__, prop);

	return fdt32_to_cpu(*ac);
}

static void fdt_read_string(const void *fdt, int nodeoffset, const char *prop, char *s, size_t maxlen)
{
	const fdt32_t *ac;
	int len;

	ac = fdt_getprop(fdt, nodeoffset, prop, &len);
	if (!ac)
		panic("%s: no %s property\n", __func__, prop);

	strncpy(s, (void *)ac, maxlen);
}

static void fdt_read_array(const void *fdt, int nodeoffset, const char *prop, void *a, size_t nelm)
{
	int err =  fdtdec_get_int_array(fdt, nodeoffset, prop, a, nelm);

	if (err)
		panic("%s: no %s property\n", __func__, prop);
}

int dram_init(void)
{
	struct ddr_config config = { };
	const void *fdt = gd->fdt_blob;
	int node, ret;

	node = fdt_node_offset_by_compatible(fdt, -1, "microchip,ddr-umctl");
	if (node < 0)
		panic("%s: Cannot read DDR node in DT\n", __func__);

	/* Top-level config data */
	config.info.size = fdt_read_uint32(fdt, node, "microchip,mem-size");
	config.info.speed = fdt_read_uint32(fdt, node, "microchip,mem-speed");
	config.info.bus_width = fdt_read_uint32(fdt, node, "microchip,mem-bus-width");
	fdt_read_string(fdt, node, "microchip,mem-name", config.info.name, sizeof(config.info.name));

	/* Read param grups */
	fdt_read_array(fdt, node, "microchip,main-reg", &config.main, sizeof(config.main) / sizeof(uint32_t));
	fdt_read_array(fdt, node, "microchip,timing-reg", &config.timing, sizeof(config.timing) / sizeof(uint32_t));
	fdt_read_array(fdt, node, "microchip,mapping-reg", &config.mapping, sizeof(config.mapping) / sizeof(uint32_t));
	fdt_read_array(fdt, node, "microchip,phy-reg", &config.phy, sizeof(config.phy) / sizeof(uint32_t));
	fdt_read_array(fdt, node, "microchip,phy_timing-reg", &config.phy_timing, sizeof(config.phy_timing) / sizeof(uint32_t));

	ret = ddr_init(&config);
	if (ret)
		panic("DDR initialization returns %d\n", ret);

	memcpy((void *)VIRT_SDRAM_1, &config, sizeof(config));

	gd->ram_size = config.info.size;

	return 0;
}

void board_add_ram_info(int use_default)
{
	struct ddr_config *config = (void *)VIRT_SDRAM_1;
	int bus_width = config->info.bus_width;

	printf(" (%d MHz", config->info.speed);
	printf(", %s", (config->main.mstr & MSTR_DDR4) ? "DDR4" : "DDR3");
	if (FIELD_GET(MSTR_DATA_BUS_WIDTH, config->main.mstr) == 1) /* Half width */
		bus_width /= 2;
	if (FIELD_GET(MSTR_DATA_BUS_WIDTH, config->main.mstr) == 2) /* Quarter width */
		bus_width /= 4;
	printf(", %dbit", bus_width);
	printf(", %s", (config->main.ecccfg0 & ECCCFG0_ECC_MODE) ? "ECC" : "No ECC");
	if (config->main.mstr & MSTR_EN_2T_TIMING_MODE)
		puts(", 2T");
	if (config->info.name[0])
		printf(", %s", config->info.name);
	puts(")");
}
