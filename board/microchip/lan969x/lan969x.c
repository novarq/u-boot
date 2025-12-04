// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 */

#include <asm/io.h>
#include <asm/armv8/cpu.h>
#include <asm/armv8/mmu.h>
#include <dm/uclass.h>
#include <dm/uclass-internal.h>
#include <linux/sizes.h>
#include <asm/global_data.h>
#include <env.h>
#include <env_internal.h>

#include <asm/arch/soc.h>

DECLARE_GLOBAL_DATA_PTR;

static struct mm_region fa_mem_map[] = {
	{
		.virt = PHYS_SDRAM_1,
		.phys = PHYS_SDRAM_1,
		.size = PHYS_SDRAM_1_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		.virt = LAN969X_SRAM_BASE,
		.phys = LAN969X_SRAM_BASE,
		.size = LAN969X_SRAM_SIZE,
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
		.virt = LAN969X_USB_BASE,
		.phys = LAN969X_USB_BASE,
		.size = LAN969X_USB_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};
struct mm_region *mem_map = fa_mem_map;

int dram_init(void)
{
	gd->ram_size = tfa_get_dram_size();

	/* Fall-back to compile-time default */
	if (!gd->ram_size)
		gd->ram_size = LAN969X_DDR_SIZE_DEF;

	return 0;
}

static void add_memory_bank(int bankno, phys_addr_t start, phys_size_t size)
{
	gd->bd->bi_dram[bankno].start = start;
	gd->bd->bi_dram[bankno].size = size;
}

int dram_init_banksize(void)
{
	int bankno = 0;
	phys_addr_t start;
	phys_size_t size;

	/* Add DDR */
	add_memory_bank(bankno++, PHYS_SDRAM_1, gd->ram_size);

	/* First the lower half of SRAM */
	size = tfa_get_sram_info(0, &start);
	if (size) {
		add_memory_bank(bankno++, start, size);
	}

	/* Upper half of SRAM has 1st 128K reserved for BL31 */
	size = tfa_get_sram_info(1, &start);
	if (size) {
		add_memory_bank(bankno++, start, size);
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

int board_init(void)
{
	return 0;
}
