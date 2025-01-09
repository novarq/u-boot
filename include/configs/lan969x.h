/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LAN969X_CONFIG_H
#define __LAN969X_CONFIG_H

#include <linux/sizes.h>

/* LAN969X defines */
#define LAN969X_QSPI0_MMAP      UL(0x20000000)
#define LAN969X_QSPI0_RANGE     SZ_16M
#define LAN969X_SRAM_BASE       UL(0x00100000)
#define LAN969X_SRAM_SIZE       SZ_2M
#define LAN969X_DDR_BASE        UL(0x60000000)
#ifdef CONFIG_TARGET_LAN969X_SRAM
#define LAN969X_DDR_SIZE_DEF    0x100000
#else
#define LAN969X_DDR_SIZE_DEF    (SZ_1G - (SZ_1G / 8)) /* ECC enabled cost 1/8th capacity */
#endif
#define LAN969X_DDR_SIZE_MAX    SZ_2G

#define LAN969X_DEV_BASE	UL(0xE0000000)
#define LAN969X_DEV_SIZE	UL(0x10000000)

#define LAN969X_USB_BASE	0x300000
#define LAN969X_USB_SIZE	0x80000

#define PHYS_SDRAM_1		LAN969X_DDR_BASE
#define PHYS_SDRAM_1_SIZE	LAN969X_DDR_SIZE_MAX /* Used for MMU table - only */

#ifdef CONFIG_TARGET_LAN969X_SRAM
#define CFG_SYS_SDRAM_BASE      0x100000
#else
#define CFG_SYS_SDRAM_BASE      LAN969X_DDR_BASE
#endif
#define CFG_SYS_INIT_RAM_ADDR	(LAN969X_DDR_BASE + SZ_64M)
#define CFG_SYS_INIT_RAM_SIZE	SZ_32K

#define CFG_SYS_BOOTMAPSZ	SZ_64M	/* Initial map for Linux*/

#define CFG_ENV_CALLBACK_LIST_STATIC "mmc_cur:mmc_cur"

#define PHY_ANEG_TIMEOUT	20000

#endif	/* __LAN969X_CONFIG_H */
