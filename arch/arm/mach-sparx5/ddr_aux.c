// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2022 Microsemi Corporation
 */

#include <common.h>
#include <asm/io.h>

#include <mscc_sparx5_regs_common.h>

#define DW_APB_LOAD_VAL		(MSCC_TO_TIMERS + (0*4))
#define DW_APB_CURR_VAL		(MSCC_TO_TIMERS + (1*4))
#define DW_APB_CTRL		(MSCC_TO_TIMERS + (2*4))
#define DW_APB_EOI		(MSCC_TO_TIMERS + (3*4))
#define DW_APB_INTSTAT		(MSCC_TO_TIMERS + (4*4))

#define CLOC_SPEED	(250 * 1000000) /* 250Mhz */
#define NS_PER_TICK	(1000000000/CLOC_SPEED) /* ns = (10 ** 9), ns/tick = 4 */

void ddr_nsleep(u32 t_nsec)
{
	/* init timer */
	t_nsec /= NS_PER_TICK;
	writel(t_nsec, DW_APB_LOAD_VAL);
	writel(0xffffffff, DW_APB_CURR_VAL);
	setbits_le32(DW_APB_CTRL, 0x3);
	while (readl(DW_APB_INTSTAT) == 0)
		;
	clrbits_le32(DW_APB_CTRL, 0x3);
	(void) readl(DW_APB_EOI);
}
