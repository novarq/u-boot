// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Microsemi Corporation
 */

//#define DEBUG

#include <common.h>
#include <config.h>
#include <dm.h>
#include <dm/devres.h>
#include <dm/of_access.h>
#include <dm/of_addr.h>
#include <fdt_support.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <miiphy.h>
#include <net.h>
#include <wait_bit.h>
#include <command.h>

#include "mscc_xfer.h"
#include "mscc_miim.h"

#include "mscc_sparx5_regs_cpu.h"
#include "mscc_sparx5_regs_devcpu_gcb.h"

#include <dt-bindings/mscc/sparx5_data.h>

static struct sparx5_private *dev_priv;

#define MAC_VID			1 /* Also = FID 1 */
#define ETH_ALEN		6

#define PGID_L2_UC(priv)		(priv->data->num_ports + 0)
#define PGID_L2_MC(priv)		(priv->data->num_ports + 1)
#define PGID_BROADCAST(priv)		(priv->data->num_ports + 2)
#define PGID_HOST(priv)			(priv->data->num_ports + 3)

static const char * const sparx5_reg_names[] = {
	"ana_ac", "ana_cl", "ana_l2", "ana_l3",
	"asm", "lrn", "qfwd", "qs",
	"qsys", "rew", "vop", "dsm", "eacl",
	"vcap_super", "hsch", "port_conf",
	"port0", "port1", "port2", "port3", "port4", "port5", "port6",
	"port7", "port8", "port9", "port10", "port11", "port12", "port13",
	"port14", "port15", "port16", "port17", "port18", "port19", "port20",
	"port21", "port22", "port23", "port24", "port25", "port26", "port27",
	"port28", "port29", "port30", "port31", "port32", "port33", "port34",
	"port35", "port36", "port37", "port38", "port39", "port40", "port41",
	"port42", "port43", "port44", "port45", "port46", "port47", "port48",
	"port49", "port50", "port51", "port52", "port53", "port54", "port55",
	"port56", "port57", "port58", "port59", "port60", "port61", "port62",
	"port63", "port64",
};

enum sparx5_ctrl_regs {
	ANA_AC,
	ANA_CL,
	ANA_L2,
	ANA_L3,
	ASM,
	LRN,
	QFWD,
	QS,
	QSYS,
	REW,
	VOP,
	DSM,
	EACL,
	VCAP_SUPER,
	HSCH,
	PORT_CONF,
	__REG_MAX,
};

#define SWITCHREG(base, roff)             (base + (4*(roff)))
#define SWITCHREG_IX(t, o, g, gw, r, ro)  SWITCHREG(t, (o) + ((g) * (gw)) + (ro) + (r))

/* RAM init regs */
#define ANA_AC_RAM_INIT(base)        SWITCHREG(base, 0x33371)
#define QSYS_RAM_INIT(base)          SWITCHREG(base, 0x24a)
#define ASM_RAM_INIT(base)           SWITCHREG(base, 0x2204)
#define REW_RAM_INIT(base)           SWITCHREG(base, 0x171d2)
#define DSM_RAM_INIT(base)           SWITCHREG(base, 0x0)
#define EACL_RAM_INIT(base)          SWITCHREG(base, 0x73f4)
#define VCAP_SUPER_RAM_INIT(base)    SWITCHREG(base, 0x118)
#define VOP_RAM_INIT(base)           SWITCHREG(base, 0x110a2)

/* Switch registers */
#define ANA_AC_STAT_PORT_STAT_RESET(base) SWITCHREG(base, 0x33f9c)
#define ANA_AC_PGID_CFG(base, gi)         SWITCHREG_IX(base,0x30000,gi,4,0,0)
#define ANA_AC_PGID_CFG1(base, gi)        SWITCHREG_IX(base,0x30000,gi,4,0,1)
#define ANA_AC_PGID_CFG2(base, gi)        SWITCHREG_IX(base,0x30000,gi,4,0,2)
#define ANA_AC_PGID_MISC_CFG(base, gi)    SWITCHREG_IX(base,0x30000,gi,4,0,3)
#define ANA_AC_SRC_CFG0(base, gi)	  SWITCHREG_IX(base,0x33e00,gi,4,0,0)
#define ANA_AC_SRC_CFG1(base, gi)	  SWITCHREG_IX(base,0x33e00,gi,4,0,1)
#define ANA_AC_SRC_CFG2(base, gi)	  SWITCHREG_IX(base,0x33e00,gi,4,0,2)
#define ASM_STAT_CFG(base)                SWITCHREG(base, 0x2080)
#define QSYS_CAL_AUTO(base, ri)           SWITCHREG(base, 0x240 + (ri))
#define QSYS_CAL_CTRL(base)               SWITCHREG(base, 0x249)
#define  QSYS_CAL_CTRL_CAL_MODE(x)               (GENMASK(14,11) & ((x) << 11))
#define  QSYS_CAL_CTRL_CAL_MODE_MASK             GENMASK(14,11)
#define ASM_PORT_CFG(base, ri)            SWITCHREG(base, 0x2107 + (ri))
#define  ASM_PORT_CFG_PAD_ENA		         BIT(6)
#define  ASM_PORT_CFG_NO_PREAMBLE_ENA            BIT(9)
#define  ASM_PORT_CFG_INJ_FORMAT_CFG(x)          (GENMASK(3,2) & ((x) << 2))
#define QS_INJ_GRP_CFG(base, ri)          SWITCHREG(base, 0x9 + (ri))
#define QS_XTR_GRP_CFG(base, ri)          SWITCHREG(base, 0x0 + (ri))
#define QFWD_SWITCH_PORT_MODE(base, ri)   SWITCHREG(base, 0x0 + (ri))
#define  QFWD_SWITCH_PORT_MODE_PORT_ENA          BIT(19)
#define ANA_L2_FWD_CFG(base)              SWITCHREG(base, 0x228c2)
#define  ANA_L2_FWD_CFG_CPU_DMAC_COPY_ENA        BIT(6)
#define ANA_L2_LRN_CFG(base)              SWITCHREG(base, 0x228c3)
#define  ANA_L2_LRN_CFG_VSTAX_BASIC_LRN_MODE_ENA BIT(16)
#define DEV1G_PCS1G_CFG(target)           SWITCHREG(target, 0x16)
#define  DEV1G_PCS1G_CFG_PCS_ENA                 BIT(0)
#define DEV1G_USXGMII_PCS_SD_CFG(target)  SWITCHREG(target, 0xb)
#define DEV1G_MAC_ENA_CFG(target)         SWITCHREG(target, 0xd)
#define  DEV1G_MAC_ENA_CFG_TX_ENA                BIT(0)
#define  DEV1G_MAC_ENA_CFG_RX_ENA                BIT(4)
#define DEV1G_PCS1G_MODE_CFG(target)      SWITCHREG(target, 0x17)
#define  DEV1G_PCS1G_MODE_CFG_SGMII_MODE_ENA     BIT(0)
#define DEV1G_PCS1G_ANEG_CFG(target)      SWITCHREG(target, 0x19)
#define  DEV1G_PCS1G_ANEG_CFG_ADV_ABILITY(x)     (GENMASK(31,16) & ((x) << 16))
#define  DEV1G_PCS1G_ANEG_CFG_SW_RESOLVE_ENA        BIT(8)
#define  DEV1G_PCS1G_ANEG_CFG_ANEG_RESTART_ONE_SHOT BIT(1)
#define  DEV1G_PCS1G_ANEG_CFG_ANEG_ENA              BIT(0)
#define DEV1G_PCS1G_ANEG_STATUS(target)   SWITCHREG(target, 0x1e)
#define  DEV1G_PCS1G_ANEG_STATUS_ANEG_COMPLETE	    BIT(0)
#define DEV1G_PCS1G_LINK_STATUS(target)	  SWITCHREG(target,0x20)
#define  DEV1G_PCS1G_LINK_STATUS_LINK_UP	    BIT(4)
#define DEV1G_MAC_IFG_CFG(target)         SWITCHREG(target, 0x13)
#define  DEV1G_MAC_IFG_CFG_TX_IFG(x)             (GENMASK(12,8) & ((x) << 8))
#define  DEV1G_MAC_IFG_CFG_RX_IFG2(x)            (GENMASK(7,4) & ((x) << 4))
#define  DEV1G_MAC_IFG_CFG_RX_IFG1(x)            (GENMASK(3,0) & ((x) << 0))
#define DEV1G_DEV_RST_CTRL(target)        SWITCHREG(target, 0x0)
#define  DEV1G_DEV_RST_CTRL_SPEED_SEL(x)         (GENMASK(22,20) & ((x) << 20))
#define  DEV1G_DEV_RST_CTRL_USX_PCS_TX_RST       BIT(17)
#define  DEV1G_DEV_RST_CTRL_USX_PCS_RX_RST       BIT(16)
#define  DEV1G_DEV_RST_CTRL_PCS_TX_RST           BIT(12)
#define ANA_CL_VLAN_CTRL(base, gi)        SWITCHREG_IX(base,0x8000,gi,128,0,8)
#define  ANA_CL_VLAN_CTRL_VLAN_AWARE_ENA         BIT(19)
#define  ANA_CL_VLAN_CTRL_VLAN_POP_CNT(x)        (GENMASK(18,17) & ((x) << 17))
#define ANA_CL_CAPTURE_BPDU_CFG(target, gi) SWITCHREG_IX(target,0x8000,gi,128,0,49)
#define ANA_CL_FILTER_CTRL(target, gi)      SWITCHREG_IX(target,0x8000,gi,128,0,1)
#define  ANA_CL_FILTER_CTRL_FORCE_FCS_UPDATE_ENA BIT(0)
#define LRN_COMMON_ACCESS_CTRL(base)      SWITCHREG(base, 0x0)
#define  LRN_COMMON_ACCESS_CTRL_CPU_ACCESS_CMD(x)  (GENMASK(4,1) & ((x) << 1))
#define  LRN_COMMON_ACCESS_CTRL_MAC_TABLE_ACCESS_SHOT     BIT(0)
#define LRN_MAC_ACCESS_CFG0(base)         SWITCHREG(base, 0x1)
#define LRN_MAC_ACCESS_CFG1(base)         SWITCHREG(base, 0x2)
#define LRN_MAC_ACCESS_CFG2(base)         SWITCHREG(base, 0x3)
#define  LRN_MAC_ACCESS_CFG2_MAC_ENTRY_ADDR(x)     (GENMASK(11,0) & ((x) << 0))
#define  LRN_MAC_ACCESS_CFG2_MAC_ENTRY_ADDR_MASK   GENMASK(11,0)
#define  LRN_MAC_ACCESS_CFG2_MAC_ENTRY_TYPE(x)     (GENMASK(14,12) & ((x) << 12))
#define  LRN_MAC_ACCESS_CFG2_MAC_ENTRY_VLD         BIT(15)
#define  LRN_MAC_ACCESS_CFG2_MAC_ENTRY_LOCKED      BIT(16)
#define  LRN_MAC_ACCESS_CFG2_MAC_ENTRY_CPU_COPY    BIT(23)
#define  LRN_MAC_ACCESS_CFG2_MAC_ENTRY_CPU_QU(x)   (GENMASK(26,24) & ((x) << 24))
#define LRN_SCAN_NEXT_CFG(base)           SWITCHREG(base, 0x5)
#define  LRN_SCAN_NEXT_CFG_UNTIL_FOUND_ENA         BIT(10)
#define  LRN_SCAN_NEXT_CFG_IGNORE_LOCKED_ENA	   BIT(7)
#define HSCH_RESET_CFG(base)              SWITCHREG(base, 0x9e92)
#define PORT_CONF_DEV5G_MODES(base)       SWITCHREG(base, 0x0)
#define  PORT_CONF_DEV5G_MODES_P64_SGMII           BIT(12)
#define  PORT_CONF_DEV5G_MODES_P0_11		   GENMASK(11,0)
#define PORT_CONF_DEV10G_MODES(base)      SWITCHREG(base, 0x1)
#define PORT_CONF_QSGMII_ENA(base)      SWITCHREG(base, 0x3)
#define PORT_CONF_USGMII_CFG(base, gi)        SWITCHREG_IX(base,0x12,gi,2,0,0)

#define DSM_DEV_TX_STOP_WM_CFG(base, ri)  SWITCHREG(base, 0x159 + (ri))
#define  DSM_DEV_TX_STOP_WM_CFG_DEV10G_SHADOW_ENA  BIT(8)
#define ANA_L3_VLAN_CTRL(base)            SWITCHREG(base, 0x1e211)
#define ANA_L3_VLAN_CFG(base, gi)         SWITCHREG_IX(base,0x0,gi,16,0,2)

static const unsigned long sparx5_regs_qs[] = {
	[MSCC_QS_XTR_RD] = 0x8,
	[MSCC_QS_XTR_FLUSH] = 0x18,
	[MSCC_QS_XTR_DATA_PRESENT] = 0x1c,
	[MSCC_QS_INJ_WR] = 0x2c,
	[MSCC_QS_INJ_CTRL] = 0x34,
};

#define IFH_FMT_NONE  0		/* No IFH */

#define CONFIG_IFH_FMT     IFH_FMT_NONE
#define CONFIG_VLAN_ENABLE
//#define CONFIG_CPU_PGID_ENA
//#define CONFIG_CPU_BPDU_ENA
#define CONFIG_CPU_MACENTRY_ENA

enum {
	SERDES_ARG_MAC_TYPE,
	SERDES_ARG_SERDES,
	SERDES_ARG_SER_IDX,
	SERDES_ARG_MAX,
};

enum {
	SPARX5_PCB_UNKNOWN,
	SPARX5_PCB_134 = 134,
	SPARX5_PCB_135 = 135,
};

struct mscc_match_data {
	const char * const *reg_names;
	u8 num_regs;
	u8 num_ports;
	u8 num_bus;
	u8 cpu_port;
	u8 npi_port;
	u8 ifh_len;
};

static struct mscc_match_data mscc_sparx5_data = {
	.reg_names = sparx5_reg_names,
	.num_regs = 81,
	.num_ports = 65,
	.num_bus = 4,
	.cpu_port = 65,
	.npi_port = 64,
	.ifh_len = 9,
};

struct sparx5_phy_port {
	bool active;
	struct mii_dev *bus;
	u8 phy_addr;
	struct phy_device *phy;
	u32 mac_type;
	u32 serdes_type;
	u32 serdes_index;
};

struct sparx5_private {
	struct mscc_match_data *data;

	void __iomem **regs;
	struct mii_dev **bus;
	struct mscc_miim_dev *miim;
	struct sparx5_phy_port *ports;

	u32 pcb;
};

extern void sparx5_serdes_port_init(int port,
				     u32 mac_type,
				     u32 serdes_type,
				     u32 serdes_idx);

extern void sparx5_serdes_cmu_init(void);

extern void sparx5_serdes_init(void);

static void hexdump(u8 *buf, int len)
{
#if 0
	int i;

	len = min(len, 60);
	for (i = 0; i < len; i++) {
		if ((i % 16) == 0)
			printf("%s%04x: ", i ? "\n" : "", i);
		printf("%02x ", buf[i]);
	}
	printf("\n");
#endif
}

static int ram_init(void __iomem *addr)
{
	writel(BIT(1), addr);

	if (wait_for_bit_le32(addr, BIT(1), false, 2000, false)) {
		printf("Timeout in memory reset, addr: %p = 0x%08x\n", addr, readl(addr));
		return 1;
	}

	return 0;
}

static bool has_link(struct sparx5_private *priv, int port)
{
	u32 mask, val;
	if (priv->ports[port].bus) {
		val = readl(DEV1G_PCS1G_LINK_STATUS(priv->regs[port + __REG_MAX]));
		mask = DEV1G_PCS1G_LINK_STATUS_LINK_UP;
	} else {
		val = readl(DEV1G_PCS1G_ANEG_STATUS(priv->regs[port + __REG_MAX]));
		mask = DEV1G_PCS1G_ANEG_STATUS_ANEG_COMPLETE;
	}

	return !!(val & mask);
}

static int sparx5_switch_init(struct sparx5_private *priv)
{
	debug("%s\n", __FUNCTION__);

	/* Initialize memories */
	ram_init(QSYS_RAM_INIT(priv->regs[QSYS]));
	ram_init(ASM_RAM_INIT(priv->regs[ASM]));
	ram_init(ANA_AC_RAM_INIT(priv->regs[ANA_AC]));
	ram_init(REW_RAM_INIT(priv->regs[REW]));
	ram_init(DSM_RAM_INIT(priv->regs[DSM]));
	ram_init(EACL_RAM_INIT(priv->regs[EACL]));
	ram_init(VCAP_SUPER_RAM_INIT(priv->regs[VCAP_SUPER]));
	ram_init(VOP_RAM_INIT(priv->regs[VOP]));

	/* Reset counters */
	writel(0x1, ANA_AC_STAT_PORT_STAT_RESET(priv->regs[ANA_AC]));
	writel(0x1, ASM_STAT_CFG(priv->regs[ASM]));

	return 0;
}

static void sparx5_switch_config(struct sparx5_private *priv)
{
	int i;

	debug("%s\n", __FUNCTION__);

	/* Halt the calendar while changing it */
	clrsetbits_le32(QSYS_CAL_CTRL(priv->regs[QSYS]),
			QSYS_CAL_CTRL_CAL_MODE_MASK,
			QSYS_CAL_CTRL_CAL_MODE(10));

	for (i = 0; i < 7; i++)
		/* All ports to '001' - 1Gb/s */
		writel(01111111111, QSYS_CAL_AUTO(priv->regs[QSYS], i));

	/* Enable Auto mode */
	clrsetbits_le32(QSYS_CAL_CTRL(priv->regs[QSYS]),
			QSYS_CAL_CTRL_CAL_MODE_MASK,
			QSYS_CAL_CTRL_CAL_MODE(8));

	/* Configure NPI port */
	if (priv->ports[priv->data->npi_port].mac_type == IF_SGMII)
		setbits_le32(PORT_CONF_DEV5G_MODES(priv->regs[PORT_CONF]),
			     PORT_CONF_DEV5G_MODES_P64_SGMII);
	else
		clrbits_le32(PORT_CONF_DEV5G_MODES(priv->regs[PORT_CONF]),
			     PORT_CONF_DEV5G_MODES_P64_SGMII);

	/* Enable all 10G ports */
	writel(0xFFF, PORT_CONF_DEV10G_MODES(priv->regs[PORT_CONF]));

	for (i = 0; i < priv->data->num_ports; i++) {
		struct sparx5_phy_port *p = &priv->ports[i];
		/* Enable 10G shadow interfaces */
		if (p->active)
			setbits_le32(DSM_DEV_TX_STOP_WM_CFG(priv->regs[DSM], i),
				     DSM_DEV_TX_STOP_WM_CFG_DEV10G_SHADOW_ENA);

		/* Enable shadow 5G interfaces */
		if (p->active && (p->mac_type == IF_QSGMII) && (i < 12))
			setbits_le32(PORT_CONF_DEV5G_MODES(priv->regs[PORT_CONF]), BIT(i));

		/* MUXing for QSGMII */
		if (p->active && (p->mac_type == IF_QSGMII)) {
			setbits_le32(PORT_CONF_QSGMII_ENA(priv->regs[PORT_CONF]), BIT((i - i % 4) / 4));
		}

		if (p->active && (p->mac_type == IF_QSGMII)) {
			if ((i / 4 % 2) == 0) {
				/* Affects d0-d3,d8-d11..d40-d43 */
				writel(0x332, PORT_CONF_USGMII_CFG(priv->regs[PORT_CONF], i/8));
			}
		}

		if (p->active && (p->mac_type == IF_QSGMII)) {
			u32 p = (i / 4) * 4;
			for (u32 cnt = 0; cnt < 4; cnt++) {
				/* Must take the PCS out of reset for all 4 QSGMII instances */
				clrbits_le32(DEV1G_DEV_RST_CTRL(priv->regs[p + cnt]),
							 DEV1G_DEV_RST_CTRL_PCS_TX_RST);
			}
		}
	}

	if (priv->pcb == SPARX5_PCB_134) {
		/* SGPIO HACK */
		writel(0x00060051, MSCC_DEVCPU_GCB_SIO_CFG(2));
		writel(0x00001410, MSCC_DEVCPU_GCB_SIO_CLOCK(2));
		writel(0xfffff000, MSCC_DEVCPU_GCB_SIO_PORT_ENA(2));
		writel(0x00000fff, MSCC_DEVCPU_GCB_SIO_INTR_POL(2, 0));
		writel(0xffffffff, MSCC_DEVCPU_GCB_SIO_INTR_POL(2, 1));
		writel(0x00000fff, MSCC_DEVCPU_GCB_SIO_INTR_POL(2, 2));
		writel(0xffffffff, MSCC_DEVCPU_GCB_SIO_INTR_POL(2, 3));
		for (i = 12; i < 32; i++)
			writel(0x00049000, MSCC_DEVCPU_GCB_SIO_PORT_CFG(2, i));
	}

	/* BCAST/CPU pgid */
	writel(0xffffffff, ANA_AC_PGID_CFG(priv->regs[ANA_AC], PGID_BROADCAST(priv)));
	writel(0xffffffff, ANA_AC_PGID_CFG1(priv->regs[ANA_AC], PGID_BROADCAST(priv)));
	writel(0x00000001, ANA_AC_PGID_CFG2(priv->regs[ANA_AC], PGID_BROADCAST(priv)));
	writel(0x00000000, ANA_AC_PGID_CFG(priv->regs[ANA_AC], PGID_HOST(priv)));
	writel(0x00000000, ANA_AC_PGID_CFG1(priv->regs[ANA_AC], PGID_HOST(priv)));
	writel(0x00000000, ANA_AC_PGID_CFG2(priv->regs[ANA_AC], PGID_HOST(priv)));

	/*
	 * Disable port-to-port by switching
	 * Put front ports in "port isolation modes" - i.e. they can't send
	 * to other ports - via the PGID sorce masks.
	 */
	for (i = 0; i < priv->data->num_ports; i++) {
		writel(0, ANA_AC_SRC_CFG0(priv->regs[ANA_AC], i));
		writel(0, ANA_AC_SRC_CFG1(priv->regs[ANA_AC], i));
		writel(0, ANA_AC_SRC_CFG2(priv->regs[ANA_AC], i));
	}

#if defined(CONFIG_CPU_PGID_ENA)
	/* CPU copy UC+MC:FLOOD */
	writel(1, ANA_AC_PGID_MISC_CFG(priv->regs[ANA_AC], PGID_L2_MC(priv)));
	writel(1, ANA_AC_PGID_MISC_CFG(priv->regs[ANA_AC], PGID_L2_UC(priv)));
#endif

	/* HACK: Convenience XQS XQS:SYSTEM:STAT_CFG */
	writel(priv->data->cpu_port << 5, 0x6110c1dcc);

#if defined(CONFIG_VLAN_ENABLE)
	/* VLAN aware CPU port */
	writel(ANA_CL_VLAN_CTRL_VLAN_AWARE_ENA |
	       ANA_CL_VLAN_CTRL_VLAN_POP_CNT(1) |
	       MAC_VID,
	       ANA_CL_VLAN_CTRL(priv->regs[ANA_CL], priv->data->cpu_port));
	/* XXX: Map PVID = FID, DISABLE LEARNING  */
	writel((MAC_VID << 8) | BIT(3),
	       ANA_L3_VLAN_CFG(priv->regs[ANA_L3], MAC_VID));
	/* Enable VLANs */
	writel(1, ANA_L3_VLAN_CTRL(priv->regs[ANA_L3]));
#endif

	/* Enable switch-core and queue system */
	writel(0x1, HSCH_RESET_CFG(priv->regs[HSCH]));

	/* Flush queues */
	mscc_flush(priv->regs[QS], sparx5_regs_qs);
}

static void sparx5_cpu_capture_setup(struct sparx5_private *priv)
{
	debug("%s\n", __FUNCTION__);

	/* ASM CPU port: No preamble/IFH, enable padding */
	writel(ASM_PORT_CFG_PAD_ENA |
	       ASM_PORT_CFG_NO_PREAMBLE_ENA |
	       ASM_PORT_CFG_INJ_FORMAT_CFG(CONFIG_IFH_FMT),
	       ASM_PORT_CFG(priv->regs[ASM], priv->data->cpu_port));

	/* Set Manual injection via DEVCPU_QS registers for CPU queue 0 */
	writel(0x5, QS_INJ_GRP_CFG(priv->regs[QS], 0));

	/* Set Manual extraction via DEVCPU_QS registers for CPU queue 0 */
	writel(0x7, QS_XTR_GRP_CFG(priv->regs[QS], 0));

	/* Enable CPU port for any frame transfer */
	setbits_le32(QFWD_SWITCH_PORT_MODE(priv->regs[QFWD], priv->data->cpu_port),
		     QFWD_SWITCH_PORT_MODE_PORT_ENA);

	/* Recalc injected frame FCS */
	setbits_le32(ANA_CL_FILTER_CTRL(priv->regs[ANA_CL], priv->data->cpu_port),
		     ANA_CL_FILTER_CTRL_FORCE_FCS_UPDATE_ENA);

#if 0
	/* Enable basic learning mode */
	setbits_le32(ANA_L2_LRN_CFG(priv->regs[ANA_L2]),
		     ANA_L2_LRN_CFG_VSTAX_BASIC_LRN_MODE_ENA);
#endif

	/* Send a copy to CPU when found as forwarding entry */
	setbits_le32(ANA_L2_FWD_CFG(priv->regs[ANA_L2]),
		     ANA_L2_FWD_CFG_CPU_DMAC_COPY_ENA);
}

static void sparx5_port_init(struct sparx5_private *priv, int port, u32 mac_if)
{
	void __iomem *regs = priv->regs[port + __REG_MAX];

	debug("%s: port %d\n", __FUNCTION__, port);

	sparx5_serdes_port_init(port,
				 priv->ports[port].mac_type,
				 priv->ports[port].serdes_type,
				 priv->ports[port].serdes_index);
	/* Enable PCS */
	writel(DEV1G_PCS1G_CFG_PCS_ENA, DEV1G_PCS1G_CFG(regs));

	/* Disable Signal Detect */
	writel(0, DEV1G_USXGMII_PCS_SD_CFG(regs));

	/* Enable MAC RX and TX */
	writel(DEV1G_MAC_ENA_CFG_TX_ENA |
	       DEV1G_MAC_ENA_CFG_RX_ENA,
	       DEV1G_MAC_ENA_CFG(regs));


	/* Enable sgmii_mode_ena */
	writel(DEV1G_PCS1G_MODE_CFG_SGMII_MODE_ENA, DEV1G_PCS1G_MODE_CFG(regs));

	if (mac_if == IF_SGMII || IF_QSGMII) {
		/*
		 * Clear sw_resolve_ena(bit 0) and set adv_ability to
		 * something meaningful just in case
		 */
		writel(DEV1G_PCS1G_ANEG_CFG_ADV_ABILITY(0x20),
		       DEV1G_PCS1G_ANEG_CFG(regs));
	} else {
		/* IF_SGMII_CISCO */
		writel(DEV1G_PCS1G_ANEG_CFG_ADV_ABILITY(0x0001) |
		       DEV1G_PCS1G_ANEG_CFG_SW_RESOLVE_ENA |
		       DEV1G_PCS1G_ANEG_CFG_ANEG_ENA |
		       DEV1G_PCS1G_ANEG_CFG_ANEG_RESTART_ONE_SHOT,
		       DEV1G_PCS1G_ANEG_CFG(regs));
	}

	/* Set MAC IFG Gaps */
	writel(DEV1G_MAC_IFG_CFG_TX_IFG(4) |
	       DEV1G_MAC_IFG_CFG_RX_IFG1(5) |
	       DEV1G_MAC_IFG_CFG_RX_IFG2(1),
	       DEV1G_MAC_IFG_CFG(regs));

	/* Set link speed and release all resets but USX */
	writel(DEV1G_DEV_RST_CTRL_SPEED_SEL(2) |
	       DEV1G_DEV_RST_CTRL_USX_PCS_TX_RST |
	       DEV1G_DEV_RST_CTRL_USX_PCS_RX_RST,
	       DEV1G_DEV_RST_CTRL(regs));


#if defined(CONFIG_VLAN_ENABLE)
	/* Make VLAN aware for CPU traffic */
	writel(ANA_CL_VLAN_CTRL_VLAN_AWARE_ENA |
	       ANA_CL_VLAN_CTRL_VLAN_POP_CNT(1) |
	       MAC_VID,
	       ANA_CL_VLAN_CTRL(priv->regs[ANA_CL], port));
#endif

#if defined(CONFIG_CPU_BPDU_ENA)
	/* Enable BPDU redirect for debugging */
	writel(0x55555555, ANA_CL_CAPTURE_BPDU_CFG(priv->regs[ANA_CL], port));
#endif

	/* Enable CPU port for any frame transfer */
	setbits_le32(QFWD_SWITCH_PORT_MODE(priv->regs[QFWD], port),
		     QFWD_SWITCH_PORT_MODE_PORT_ENA);
}

static inline int sparx5_vlant_wait_for_completion(struct sparx5_private *priv)
{
	if (wait_for_bit_le32(LRN_COMMON_ACCESS_CTRL(priv->regs[LRN]),
			      LRN_COMMON_ACCESS_CTRL_MAC_TABLE_ACCESS_SHOT,
			      false, 2000, false))
		return -ETIMEDOUT;

	return 0;
}

static void mac_table_write_entry(struct sparx5_private *priv,
				  const unsigned char *mac,
				  u16 vid)
{
	u32 macl = 0, mach = 0;

	/*
	 * Set the MAC address to handle and the vlan associated in a format
	 * understood by the hardware.
	 */
	mach |= vid << 16;
	mach |= ((u32)mac[0]) << 8;
	mach |= ((u32)mac[1]) << 0;
	macl |= ((u32)mac[2]) << 24;
	macl |= ((u32)mac[3]) << 16;
	macl |= ((u32)mac[4]) << 8;
	macl |= ((u32)mac[5]) << 0;

	writel(mach, LRN_MAC_ACCESS_CFG0(priv->regs[LRN]));
	writel(macl, LRN_MAC_ACCESS_CFG1(priv->regs[LRN]));
}

static int sparx5_mac_table_add(struct sparx5_private *priv,
				 const unsigned char *mac, int pgid)
{
#if defined(CONFIG_CPU_MACENTRY_ENA)
	debug("%s: Add %02x:%02x:%02x:%02x:%02x:%02x pgid %d\n",
	      __FUNCTION__,
	      mac[0], mac[1], mac[2],
	      mac[3], mac[4], mac[5], pgid);

	mac_table_write_entry(priv, mac, MAC_VID);

	writel(LRN_MAC_ACCESS_CFG2_MAC_ENTRY_ADDR(pgid - priv->data->num_ports) |
	       LRN_MAC_ACCESS_CFG2_MAC_ENTRY_TYPE(0x3) |
	       LRN_MAC_ACCESS_CFG2_MAC_ENTRY_CPU_COPY |
	       LRN_MAC_ACCESS_CFG2_MAC_ENTRY_CPU_QU(0) |
	       LRN_MAC_ACCESS_CFG2_MAC_ENTRY_VLD |
	       LRN_MAC_ACCESS_CFG2_MAC_ENTRY_LOCKED,
	       LRN_MAC_ACCESS_CFG2(priv->regs[LRN]));

	writel(LRN_COMMON_ACCESS_CTRL_MAC_TABLE_ACCESS_SHOT,
	       LRN_COMMON_ACCESS_CTRL(priv->regs[LRN]));

	return sparx5_vlant_wait_for_completion(priv);
#else
	return 0;
#endif
}

static int sparx5_mac_table_getnext(struct sparx5_private *priv,
				     unsigned char *mac,
				     int *addr,
				     u32 *pvid,
				     u32 *cfg0p,
				     u32 *cfg1p,
				     u32 *cfg2p)
{
	int ret;

	//debug("%s: start\n", __FUNCTION__);

	mac_table_write_entry(priv, mac, *pvid);

	writel(LRN_SCAN_NEXT_CFG_UNTIL_FOUND_ENA |
	       LRN_SCAN_NEXT_CFG_IGNORE_LOCKED_ENA,
	       LRN_SCAN_NEXT_CFG(priv->regs[LRN]));

	writel(LRN_COMMON_ACCESS_CTRL_CPU_ACCESS_CMD(6) |
	       LRN_COMMON_ACCESS_CTRL_MAC_TABLE_ACCESS_SHOT,
	       LRN_COMMON_ACCESS_CTRL(priv->regs[LRN]));

	ret = sparx5_vlant_wait_for_completion(priv);

	if (ret == 0) {
		u32 cfg0, cfg1, cfg2;
		cfg2 = readl(LRN_MAC_ACCESS_CFG2(priv->regs[LRN]));
		if (cfg2 & LRN_MAC_ACCESS_CFG2_MAC_ENTRY_VLD) {
			cfg0 = readl(LRN_MAC_ACCESS_CFG0(priv->regs[LRN]));
			cfg1 = readl(LRN_MAC_ACCESS_CFG1(priv->regs[LRN]));
			mac[0] = ((cfg0 >> 8)  & 0xff);
			mac[1] = ((cfg0 >> 0)  & 0xff);
			mac[2] = ((cfg1 >> 24) & 0xff);
			mac[3] = ((cfg1 >> 16) & 0xff);
			mac[4] = ((cfg1 >> 8)  & 0xff);
			mac[5] = ((cfg1 >> 0)  & 0xff);
			*addr = LRN_MAC_ACCESS_CFG2_MAC_ENTRY_ADDR_MASK & cfg2;
			*pvid = cfg0 >> 16;
			*cfg0p = cfg0;
			*cfg1p = cfg1;
			*cfg2p = cfg2;
		} else {
			ret = 1;
		}
	}

	//debug("%s: ret %d\n", __FUNCTION__, ret);

	return ret;
}

static int sparx5_initialize(struct sparx5_private *priv)
{
	int ret, i;

	debug("%s\n", __FUNCTION__);

	/* Initialize switch memories, enable core */
	ret = sparx5_switch_init(priv);
	if (ret)
		return ret;

	sparx5_switch_config(priv);

	for (i = 0; i < priv->data->num_ports; i++)
		if (priv->ports[i].active)
			sparx5_port_init(priv, i, priv->ports[i].mac_type);

	sparx5_cpu_capture_setup(priv);

	return 0;
}

static int sparx5_start(struct udevice *dev)
{
	struct sparx5_private *priv = dev_get_priv(dev);
	struct eth_pdata *pdata = dev_get_plat(dev);
	const u8 mac[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	int i, ret, phy_ok = 0, ret_err = 0;

	debug("%s\n", __FUNCTION__);

	ret = sparx5_initialize(priv);
	if (ret)
		return ret;

	/* Set MAC address tables entries for CPU redirection */
	ret = sparx5_mac_table_add(priv, mac, PGID_BROADCAST(priv));
	if (ret)
		return ret;

	ret = sparx5_mac_table_add(priv, pdata->enetaddr, PGID_HOST(priv));
	if (ret)
		return ret;

	for (i = 0; i < priv->data->num_ports; i++)
		if (priv->ports[i].active) {
			struct phy_device *phy = priv->ports[i].phy;

			if (phy) {
				/* Start up the PHY */
				ret = phy_startup(phy);
				if (ret) {
					printf("Could not initialize PHY %s (port %d)\n",
						   phy->dev->name, i);
					ret_err = ret;
					continue; /* try all phys */
				} else {
					phy_ok = 1;
					if (i == priv->data->npi_port)
						printf("NPI Port: ");
					else
						printf("Port %3d: ", i);

					printf("%s (internal)\n", has_link(priv, i) ? "Up" : "Down");
				}
			}
		}

	return phy_ok ? 0 : ret_err;
}

static void sparx5_stop(struct udevice *dev)
{
	struct sparx5_private *priv = dev_get_priv(dev);
	int i;

	debug("%s\n", __FUNCTION__);

	for (i = 0; i < priv->data->num_ports; i++) {
		struct phy_device *phy = priv->ports[i].phy;

		if (phy)
			phy_shutdown(phy);
	}

        /* Make sure the core is PROTECTED from reset */
	setbits_le32(MSCC_CPU_RESET_PROT_STAT,
		     MSCC_M_CPU_RESET_PROT_STAT_SYS_RST_PROT_VCORE);

	/* Reset switch core */
	writel(MSCC_M_DEVCPU_GCB_SOFT_RST_SOFT_CHIP_RST, MSCC_DEVCPU_GCB_SOFT_RST);
}

static int sparx5_send(struct udevice *dev, void *packet, int length)
{
	struct sparx5_private *priv = dev_get_priv(dev);

	//debug("%s: %d bytes\n", __FUNCTION__, length);
	hexdump(packet, length);
	return mscc_send(priv->regs[QS], sparx5_regs_qs,
			 NULL, 0, packet, length);
}

static int sparx5_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct sparx5_private *priv = dev_get_priv(dev);
	u32 *rxbuf = (u32 *)net_rx_packets[0];
	int byte_cnt = 0;

	//debug("%s\n", __FUNCTION__);

	byte_cnt = mscc_recv(priv->regs[QS], sparx5_regs_qs, rxbuf,
			     priv->data->ifh_len, false);

	*packetp = net_rx_packets[0];

	if (byte_cnt > 0) {
		if (byte_cnt >= ETH_FCS_LEN) {
			byte_cnt -= ETH_FCS_LEN;
			//debug("%s: Got %d bytes\n", __FUNCTION__, byte_cnt);
			hexdump(*packetp, byte_cnt);
		} else {
			debug("%s: Got undersized frame = %d bytes\n",
			      __FUNCTION__, byte_cnt);
			byte_cnt = 0; /* Runt? */
		}
	}

	return byte_cnt;
}

static struct mii_dev *sparx5_get_mdiobus(struct sparx5_private *priv,
					   phys_addr_t base, unsigned long size)
{
	int i = 0;

	for (i = 0; i < priv->data->num_bus; ++i)
		if (priv->miim[i].miim_base == base &&
		    priv->miim[i].miim_size == size)
			return priv->miim[i].bus;

	return NULL;
}

static void sparx5_add_port_entry(struct sparx5_private *priv, size_t index,
				   size_t phy_addr, struct mii_dev *bus,
				   u32 *serdes_args)
{
	debug("%s: Add port %zd bus %s addr %zd serdes %s serdes# %d\n", __FUNCTION__, index,
	      bus ? bus->name : "(none)", phy_addr,
	      serdes_args[SERDES_ARG_SERDES] == FA_SERDES_TYPE_6G ? "6g" : "10g",
	      serdes_args[SERDES_ARG_SER_IDX]);

	priv->ports[index].active = true;
	priv->ports[index].phy_addr = phy_addr;
	priv->ports[index].bus = bus;
	priv->ports[index].mac_type = serdes_args[SERDES_ARG_MAC_TYPE];
	priv->ports[index].serdes_type = serdes_args[SERDES_ARG_SERDES];
	priv->ports[index].serdes_index = serdes_args[SERDES_ARG_SER_IDX];
}

static int sparx5_probe(struct udevice *dev)
{
	u32 serdes_args[SERDES_ARG_MAX];
	struct sparx5_private *priv;
	int miim_count = 0;
	int i;
	int ret;
	struct resource res;
	phys_addr_t addr_base;
	unsigned long addr_size;
	ofnode eth_node, node, mdio_node;
	size_t phy_addr;
	struct mii_dev *bus;
	struct ofnode_phandle_args phandle;

	debug("%s\n", __FUNCTION__);

	priv = dev_get_priv(dev);
	if (!priv)
		return -EINVAL;

	priv->data = (struct mscc_match_data*)dev_get_driver_data(dev);
	if (!priv->data)
		return -EINVAL;

	if (dev_read_u32(dev, "mscc,pcb", &priv->pcb))
		priv->pcb = SPARX5_PCB_UNKNOWN;

	/* Allocate the resources dynamically */
	priv->regs = devm_kzalloc(dev,
				  priv->data->num_regs * sizeof(void __iomem *),
				  GFP_KERNEL);
	if (!priv->regs)
		return -ENOMEM;

	priv->miim = devm_kzalloc(dev,
				  priv->data->num_bus * sizeof(struct mscc_miim_dev),
				  GFP_KERNEL);
	if (!priv->miim)
		return -ENOMEM;

	priv->bus = devm_kzalloc(dev,
				 priv->data->num_bus * sizeof(struct mii_dev*),
				 GFP_KERNEL);
	if (!priv->bus)
		return -ENOMEM;

	priv->ports = devm_kzalloc(dev,
				   priv->data->num_ports * sizeof(struct sparx5_phy_port),
				   GFP_KERNEL);
	if (!priv->ports)
		return -ENOMEM;

	/* Get registers and map them to the private structure */
	for (i = 0; i < priv->data->num_regs; i++) {
		priv->regs[i] = dev_remap_addr_name(dev,
						    priv->data->reg_names[i]);
		if (!priv->regs[i]) {
			debug
			    ("Error can't get regs base addresses for %s\n",
			     priv->data->reg_names[i]);
			return -ENOMEM;
		}
	}

	/* iterate all the ports and find out on which bus they are */
	i = 0;
	eth_node = dev_read_first_subnode(dev);
	for (node = ofnode_first_subnode(eth_node);
	     ofnode_valid(node);
	     node = ofnode_next_subnode(node)) {
		if (ofnode_read_resource(node, 0, &res))
			return -ENOMEM;
		i = res.start;

		ret = ofnode_parse_phandle_with_args(node, "phy-handle", NULL,
						     0, 0, &phandle);

		/* Do we have a PHY to worry about? */
		if (ret == 0) {
			/* Get phy address on mdio bus */
			if (ofnode_read_resource(phandle.node, 0, &res))
				return -ENOMEM;
			phy_addr = res.start;

			debug("%s: Port %d on phy %zd\n", __FUNCTION__, i, phy_addr);

			/* Get mdio node */
			mdio_node = ofnode_get_parent(phandle.node);

			if (ofnode_read_resource(mdio_node, 0, &res))
				return -ENOMEM;

			addr_base = res.start;
			addr_size = resource_size(&res);

			/* If the bus is new then create a new bus */
			if (!sparx5_get_mdiobus(priv, addr_base, addr_size))
				priv->bus[miim_count] =
					mscc_mdiobus_init(priv->miim, &miim_count, addr_base,
							  addr_size);
			//mscc_mdiobus_pinctrl_apply(mdio_node);

		} else {
			bus = NULL;
			phy_addr = -1;
		}

		/* Get serdes info */
		ret = ofnode_read_u32_array(node, "phys", serdes_args, SERDES_ARG_MAX);
		if (ret) {
			printf("%s: Port %d no 'phys' properties?\n", __FUNCTION__, i);
			return -ENOMEM;
		}

		sparx5_add_port_entry(priv, i, phy_addr, bus, serdes_args);
	}

	if (priv->pcb == SPARX5_PCB_135) {
		// Take pcb135 out of reset
		writel(0x83000, MSCC_DEVCPU_GCB_GPIO_OE);
		writel(0x80000, MSCC_DEVCPU_GCB_GPIO_OUT_CLR);
		writel(0x80000, MSCC_DEVCPU_GCB_GPIO_OUT_SET);
	}

	for (i = 0; i < priv->data->num_ports; i++) {
		struct phy_device *phy;

		if (!priv->ports[i].bus)
			continue;

		debug("%s: Port %d on %s addr %d\n", __FUNCTION__,
		      i, priv->ports[i].bus->name, priv->ports[i].phy_addr);
		phy = phy_connect(priv->ports[i].bus,
				  priv->ports[i].phy_addr, dev,
				  PHY_INTERFACE_MODE_NA);

		if (phy) {
			debug("%s: PHY %s %s\n", __FUNCTION__, phy->bus->name, phy->drv->name);
			priv->ports[i].phy = phy;

			if (i == priv->data->npi_port) {
				board_phy_config(phy);
			} else {
				if (priv->pcb == SPARX5_PCB_135) {
					/* configure Indy phy (into QSGMII mode). Elise phy not supported */
					phy_write(phy, 0, 0x16, 5);
					phy_write(phy, 0, 0x17, 0x13);
					phy_write(phy, 0, 0x16, 0x4005);
					phy_write(phy, 0, 0x17, 0x20);
				}
			}
		} else
			debug("%s: No driver\n", __FUNCTION__);
	}

	dev_priv = priv;

	/* Power down all CMUs and set all serdeses in quiet mode */
	sparx5_serdes_cmu_init();
	sparx5_serdes_init();

	return 0;
}

static int sparx5_remove(struct udevice *dev)
{
	struct sparx5_private *priv = dev_get_priv(dev);
	int i;

	for (i = 0; i < priv->data->num_bus; i++) {
		mdio_unregister(priv->bus[i]);
		mdio_free(priv->bus[i]);
	}

	dev_priv = NULL;

	return 0;
}

static const struct eth_ops sparx5_ops = {
	.start        = sparx5_start,
	.stop         = sparx5_stop,
	.send         = sparx5_send,
	.recv         = sparx5_recv,
};

static const struct udevice_id mscc_sparx5_ids[] = {
	{.compatible = "mscc,vsc7558-switch", .data = (ulong)&mscc_sparx5_data },
	{ /* Sentinel */ }
};

U_BOOT_DRIVER(sparx5) = {
	.name				= "sparx5-switch",
	.id				= UCLASS_ETH,
	.of_match			= mscc_sparx5_ids,
	.probe				= sparx5_probe,
	.remove				= sparx5_remove,
	.ops				= &sparx5_ops,
	.priv_auto			= sizeof(struct sparx5_private),
	.plat_auto			= sizeof(struct eth_pdata),
};

static int do_switch(struct cmd_tbl *cmdtp, int flag, int argc,
		     char * const argv[])
{
	struct sparx5_private *priv = dev_priv;
	u8 mac[ETH_ALEN];
	int i, addr, cnt = 0;
	u32 cfg0, cfg1, cfg2;

	if (priv) {
		memset(mac, 0, sizeof(mac));
		u32 vid = 0;
		while (sparx5_mac_table_getnext(priv, mac, &addr, &vid, &cfg0, &cfg1, &cfg2) == 0) {
			printf("%4d: %02x:%02x:%02x:%02x:%02x:%02x %d:%d 0x%08x 0x%08x 0x%08x\n",
			       cnt++,
			       mac[0], mac[1], mac[2],
			       mac[3], mac[4], mac[5], vid, addr, cfg0, cfg1, cfg2);
		}
		for (i = 0; i < priv->data->num_ports; i++)
			if (priv->ports[i].active) {
				u32 mask, val;
				if (priv->ports[i].bus) {
					val = readl(DEV1G_PCS1G_LINK_STATUS(priv->regs[i]));
					mask = DEV1G_PCS1G_LINK_STATUS_LINK_UP;
				} else {
					val = readl(DEV1G_PCS1G_ANEG_STATUS(priv->regs[i]));
					mask = DEV1G_PCS1G_ANEG_STATUS_ANEG_COMPLETE;
				}
				printf("%2d: Link %s (0x%08x)\n", i,
				       val & mask ? "Up" : "--",
				       val);
			}
	}

	return 0;
}

U_BOOT_CMD(
	switch,	3,	1,	do_switch,
	"display switch status",
	""
);
