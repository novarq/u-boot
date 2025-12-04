/*
 * Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _MICROCHIPSW_SOC_H_
#define _MICROCHIPSW_SOC_H_

#if defined(CONFIG_TARGET_LAN969X)
#include <asm/types.h>

typedef enum {
	BOOT_SOURCE_EMMC = 0,
	BOOT_SOURCE_QSPI,
	BOOT_SOURCE_SDMMC,
	BOOT_SOURCE_NONE
} boot_source_type_t;

phys_size_t tfa_get_dram_size(void);

boot_source_type_t tfa_get_boot_source(void);

int tfa_get_board_number(void);

phys_size_t tfa_get_sram_info(int ix, phys_addr_t *start);

#endif /* CONFIG_TARGET_LAN969X */
#endif /* _LAN969X_SOC_H_ */
