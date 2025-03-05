/* SPDX-License-Identifier: GPL-2.0+
 * Microchip Sparx5 SerDes driver
 *
 * Copyright (c) 2020 Microchip Technology Inc.
 */

#ifndef _SPARX5_SWITCH_H_
#define _SPARX5_SWITCH_H_

#include "sparx5_serdes.h"

#include "mscc_xfer.h"
#include "mscc_miim.h"

enum {
	SPARX5_TARGET,
	LAN969X_TARGET,
};

struct sparx5_regs {
	const unsigned int *reggrp_addr;
	const unsigned int *reggrp_cnt;
	const unsigned int *reggrp_size;
	const unsigned int *reg_addr;
	const unsigned int *reg_cnt;
	const unsigned int *regfield_addr;
};

struct mscc_match_data {
	const char * const *reg_names;
	struct sparx5_regs regs;
	u8 num_regs;
	u8 num_ports;
	u8 num_bus;
	u8 cpu_port;
	u8 npi_port;
	u8 ifh_len;
	u8 num_cal_auto;
	u8 target;
};

struct sparx5_phy_port {
	bool active;
	struct mii_dev *bus;
	u8 phy_addr;
	struct phy_device *phy;
	u32 mac_type;
	u32 serdes_type;
	u32 serdes_index;

	/* This is the serdes that correspond to the port */
	struct sparx5_serdes_phy *serdes_phy;
};

struct sparx5_private {
	struct mscc_match_data *data;

	void __iomem **regs;
	struct mii_dev **bus;
	struct mscc_miim_dev *miim;
	struct sparx5_phy_port *ports;
	struct udevice *dev;

	/* This is a container for the entire serdes, which correspond to
	 * sparx5_serdes_private
	 */
	void *serdes;
};

#endif /* _SPARX5_SWITCH_H_ */
