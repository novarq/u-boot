/* SPDX-License-Identifier: GPL-2.0+
 * Microchip Sparx5 SerDes driver
 *
 * Copyright (c) 2020 Microchip Technology Inc.
 */

#ifndef _SPARX5_SERDES_H_
#define _SPARX5_SERDES_H_

enum phy_media {
	PHY_MEDIA_DEFAULT,
	PHY_MEDIA_SR,
	PHY_MEDIA_DAC,
};

enum sparx5_serdes_type {
	SPX5_SDT_6G  = 6,
	SPX5_SDT_10G = 10,
	SPX5_SDT_25G = 25,
};

enum sparx5_serdes_mode {
	SPX5_SD_MODE_NONE,
	SPX5_SD_MODE_2G5,
	SPX5_SD_MODE_QSGMII,
	SPX5_SD_MODE_100FX,
	SPX5_SD_MODE_1000BASEX,
	SPX5_SD_MODE_SFI,
};

enum sparx5_10g28cmu_mode {
	SPX5_SD10G28_CMU_MAIN = 0,
	SPX5_SD10G28_CMU_AUX1 = 1,
	SPX5_SD10G28_CMU_AUX2 = 3,
	SPX5_SD10G28_CMU_NONE = 4,
	SPX5_SD10G28_CMU_MAX,
};

struct sparx5_serdes_macro {
	struct sparx5_serdes_private *priv;
	u32 sidx;
	u32 stpidx;
	enum sparx5_serdes_type serdestype;
	enum sparx5_serdes_mode serdesmode;
	phy_interface_t portmode;
	int speed;
	enum phy_media media;
};

struct sparx5_serdes_ops {
	void (*serdes_type_set)(struct sparx5_serdes_macro *macro, int sidx);
	int (*serdes_cmu_get)(enum sparx5_10g28cmu_mode mode, int sd_index);
};

struct sparx5_serdes_phy {
	void *data;
	struct udevice *dev;
	struct sparx5_serdes_ops ops;
};

// This is the old API just to be able to compile
void sparx5_serdes_port_init(struct sparx5_serdes_phy *phy, u32 mac_type);

// This is the new API, add more functions here
void *sparx5_serdes_probe(struct udevice *dev);
struct sparx5_serdes_phy *sparx5_serdes_phy_get(void *data, u32 serdes_type, u32 serdes_idx);
int sparx5_serdes_reset(struct sparx5_serdes_phy *phy);

#endif /* _SPARX5_SERDES_H_ */
