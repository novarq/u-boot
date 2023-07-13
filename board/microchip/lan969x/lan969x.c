// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/armv8/cpu.h>
#include <asm/armv8/mmu.h>
#include <debug_uart.h>
#include <linux/sizes.h>
#include <asm/global_data.h>
#include <env.h>
#include <env_internal.h>

#include <asm/arch/lan969x_targets_a0.h>
#include <asm/arch/lan969x_regs_a0.h>

DECLARE_GLOBAL_DATA_PTR;

enum {
	BOARD_TYPE_SUNRISE,
	BOARD_TYPE_PCB8398,
	BOARD_TYPE_PCB8422,
};

typedef enum {
	LAN966X_STRAP_BOOT_MMC_FC = 0,
        LAN966X_STRAP_BOOT_QSPI_FC = 1,
        LAN966X_STRAP_BOOT_SD_FC = 2,
        LAN966X_STRAP_BOOT_MMC = 3,
        LAN966X_STRAP_BOOT_QSPI = 4,
        LAN966X_STRAP_BOOT_SD = 5,
        LAN966X_STRAP_PCIE_ENDPOINT = 6,
        _LAN966X_STRAP_BOOT_RESERVED_7 = 7,
        LAN966X_STRAP_BOOT_QSPI_HS_FC = 8,
        _LAN966X_STRAP_BOOT_RESERVED_9 = 9,
        LAN966X_STRAP_TFAMON_FC0 = 10,
        LAN966X_STRAP_TFAMON_FC0_HS = 11,
        _LAN966X_STRAP_BOOT_RESERVED_12 = 12,
        LAN966X_STRAP_BOOT_QSPI_HS = 13,
        _LAN966X_STRAP_BOOT_RESERVED_14 = 14,
        LAN966X_STRAP_SPI_SLAVE = 15,
} soc_strapping;

typedef enum {
	BOOT_SOURCE_EMMC = 0,
	BOOT_SOURCE_QSPI,
	BOOT_SOURCE_SDMMC,
	BOOT_SOURCE_NONE
} boot_source_type;

static struct mm_region fa_mem_map[] = {
        {
                .virt = PHYS_SDRAM_1,
                .phys = PHYS_SDRAM_1,
                .size = PHYS_SDRAM_1_SIZE,
                .attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
                         PTE_BLOCK_INNER_SHARE
        }, {
                .virt = LAN969X_QSPI0_MMAP,
                .phys = LAN969X_QSPI0_MMAP,
                .size = LAN969X_QSPI0_RANGE,
                .attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
                         PTE_BLOCK_NON_SHARE |
                         PTE_BLOCK_PXN | PTE_BLOCK_UXN
        }, {
                .virt = LAN969X_DEV_BASE,
                .phys = LAN969X_DEV_BASE,
                .size = LAN969X_DEV_SIZE,
                .attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
                         PTE_BLOCK_NON_SHARE |
                         PTE_BLOCK_PXN | PTE_BLOCK_UXN
        }, {
                /* List terminator */
                0,
        }
};
struct mm_region *mem_map = fa_mem_map;

static soc_strapping lan966x_get_strapping(void)
{
	soc_strapping boot_mode;

	boot_mode = in_le32(CPU_GPR(LAN969X_CPU_BASE, 0));
	if (boot_mode == 0)
		boot_mode = in_le32(CPU_GENERAL_STAT(LAN969X_CPU_BASE));
	boot_mode = CPU_GENERAL_STAT_VCORE_CFG_X(boot_mode);

	return boot_mode;
}

static boot_source_type get_boot_source(void)
{
	boot_source_type boot_source;

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC:
	case LAN966X_STRAP_BOOT_MMC_FC:
		boot_source = BOOT_SOURCE_EMMC;
		break;
	case LAN966X_STRAP_BOOT_QSPI:
	case LAN966X_STRAP_BOOT_QSPI_FC:
	case LAN966X_STRAP_BOOT_QSPI_HS_FC:
	case LAN966X_STRAP_BOOT_QSPI_HS:
		boot_source = BOOT_SOURCE_QSPI;
		break;
	case LAN966X_STRAP_BOOT_SD:
	case LAN966X_STRAP_BOOT_SD_FC:
		boot_source = BOOT_SOURCE_SDMMC;
		break;
	default:
		boot_source = BOOT_SOURCE_NONE;
		break;
	}

	return boot_source;
}

enum env_location env_get_location(enum env_operation op, int prio)
{
	boot_source_type boot_source =  get_boot_source();

	switch(boot_source) {
	case BOOT_SOURCE_EMMC:
		return prio == 0 ? ENVL_MMC : ENVL_UNKNOWN;
	case BOOT_SOURCE_QSPI:
		return prio == 0 ? ENVL_SPI_FLASH : ENVL_UNKNOWN;
	case BOOT_SOURCE_SDMMC:
		return prio == 0 ? ENVL_FAT : ENVL_UNKNOWN;
	default:
		return ENVL_UNKNOWN;
	}
}

void reset_cpu(void)
{
	clrbits_le32(CPU_RESET_PROT_STAT(LAN969X_CPU_BASE),
		     CPU_RESET_PROT_STAT_SYS_RST_PROT_VCORE_M);

	out_le32(GCB_SOFT_RST(LAN969X_GCB_BASE), GCB_SOFT_RST_SOFT_CHIP_RST(1));
}

int print_cpuinfo(void)
{
	printf("CPU:   ARM A53\n");
	return 0;
}

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;

	return 0;
}

static void do_board_detect(void)
{
	/* CPU_BUILDID == 0 on ASIC */
	if (in_le32(CPU_BUILDID(LAN969X_CPU_BASE)) == 0) {
		/* For now, just select pcb8398 */
		gd->board_type = BOARD_TYPE_PCB8398;
	} else
		/* Sunrise FPGA board */
		gd->board_type = BOARD_TYPE_SUNRISE;
}

#if defined(CONFIG_MULTI_DTB_FIT)
int board_fit_config_name_match(const char *name)
{
	if (gd->board_type == BOARD_TYPE_SUNRISE &&
	    strcmp(name, "lan969x_sr") == 0)
		return 0;

	if (gd->board_type == BOARD_TYPE_PCB8398 &&
	    strcmp(name, "lan969x_pcb8398") == 0)
		return 0;

	if (gd->board_type == BOARD_TYPE_PCB8422 &&
	    strcmp(name, "lan969x_pcb8422") == 0)
		return 0;

	return -1;
}
#endif

#if defined(CONFIG_DTB_RESELECT)
int embedded_dtb_select(void)
{
	do_board_detect();
	fdtdec_setup();

	return 0;
}
#endif

static int lan969x_pcb8398_board_init(void)
{
	u32 val;

	/* Release the reset of the PHYs, for the lan8814 PHYs.
	 * For the lan8840 PHY, it gets out of reset when chip gets out
	 * of reset
	 */
	val = in_le32(GCB_GPIO_ALT1(LAN969X_GCB_BASE, 0));
	val &= ~BIT(62 - 32);
	out_le32(GCB_GPIO_ALT1(LAN969X_GCB_BASE, 0), val);

	val = in_le32(GCB_GPIO_ALT1(LAN969X_GCB_BASE, 1));
	val &= ~BIT(62 - 32);
	out_le32(GCB_GPIO_ALT1(LAN969X_GCB_BASE, 1), val);

	val = in_le32(GCB_GPIO_ALT1(LAN969X_GCB_BASE, 2));
	val &= ~BIT(62 - 32);
	out_le32(GCB_GPIO_ALT1(LAN969X_GCB_BASE, 2), val);

	val = in_le32(GCB_GPIO_OE1(LAN969X_GCB_BASE));
	val |= BIT(62 - 32);
	out_le32(GCB_GPIO_OE1(LAN969X_GCB_BASE), val);

	val = in_le32(GCB_GPIO_OUT_SET1(LAN969X_GCB_BASE));
	val |= BIT(62 - 32);
	out_le32(GCB_GPIO_OUT_SET1(LAN969X_GCB_BASE), val);

	return 0;
}

int board_init(void)
{
	if (gd->board_type == BOARD_TYPE_PCB8398)
		return lan969x_pcb8398_board_init();

	return 0;
}

int board_late_init(void)
{
	if (!env_get("pcb")) {
		if (gd->board_type == BOARD_TYPE_PCB8398)
			env_set("pcb", "lan9698_ung8398_0_at_lan969x");
		if (gd->board_type == BOARD_TYPE_PCB8422)
			env_set("pcb", "lan9664_ung8422_0_at_lan969x");
		if (gd->board_type == BOARD_TYPE_SUNRISE)
			env_set("pcb", "lan9664_sunrise_0_at_lan969x");
	}

	return 0;
}

int arch_cpu_init(void)
{
	return 0;
}

int mach_cpu_init(void)
{
	return 0;
}

// Helper for MMC backup image
static int on_mmc_cur(const char *name, const char *value, enum env_op op,
		      int flags)
{
	ulong mmc_cur = simple_strtoul(value, NULL, 16);

	debug("%s is %s\n", name, value);

	env_set_ulong("mmc_bak", mmc_cur == 0 ? 1 : 0);

	return 0;
}

U_BOOT_ENV_CALLBACK(mmc_cur, on_mmc_cur);
