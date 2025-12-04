#include <asm/io.h>
#include <dm/uclass.h>
#include <linux/arm-smccc.h>
#include <linux/bitfield.h>

#include <asm/arch/soc.h>

#define CPU_RESET_PROT_STAT	0xe00c0088
#define SYS_RST_PROT_VCORE_M	BIT(5)

#define GCB_CHIP_ID		0xe2010000
#define PART_ID_M		GENMASK(27, 12)

#define GCB_SOFT_RST		0xe201000c
#define CHIP_SOFT_RST_M		BIT(0)

#define SIP_SVC_UID		0x8200ff01
#define SIP_SVC_VERSION		0x8200ff02
#define SIP_SVC_GET_BOOTSRC	0x8200ff09
#define SIP_SVC_GET_DDR_SIZE	0x8200ff0a
#define SIP_SVC_GET_BOARD_NO	0x8200ff0b
#define SIP_SVC_SRAM_INFO	0x8200ff0d

enum lan969x_part_id {
	LAN9691VAO = 0x9691,  /* lan969x-40-VAO */
	LAN9692VAO = 0x9692,  /* lan969x-65-VAO */
	LAN9693VAO = 0x9693,  /* lan969x-100-VAO */
	LAN9694	   = 0x9694,  /* lan969x-40 */
	LAN9694TSN = 0x9695,  /* lan969x-40-TSN */
	LAN9694RED = 0x969A,  /* lan969x-40-RED */
	LAN9696    = 0x9696,  /* lan969x-60 */
	LAN9696TSN = 0x9697,  /* lan969x-60-TSN */
	LAN9696RED = 0x969B,  /* lan969x-60-RED */
	LAN9698    = 0x9698,  /* lan969x-100 */
	LAN9698TSN = 0x9699,  /* lan969x-100-TSN */
	LAN9698RED = 0x969C,  /* lan969x-100-RED */
};

boot_source_type_t tfa_get_boot_source(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_SVC_GET_BOOTSRC, -1, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		return BOOT_SOURCE_NONE;

	return (boot_source_type_t) res.a1;
}

phys_size_t tfa_get_dram_size(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_SVC_GET_DDR_SIZE, -1, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		return 0;

	return res.a1;
}

int tfa_get_board_number(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_SVC_GET_BOARD_NO, -1, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		return 0;

	return res.a1;
}

phys_size_t tfa_get_sram_info(int ix, phys_addr_t *start)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_SVC_VERSION, 0, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0 == 0 && res.a1 <= 1) {
		/* SRAM info supported > 0.1 */
		return 0;
	}

	arm_smccc_smc(SIP_SVC_SRAM_INFO, ix, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0) {
		/* No SRAM segment 'ix' */
		return 0;
	}

	/* Have SRAM segment */
	*start = res.a1;
	return res.a2;
}

__weak void reset_cpu(void)
{
	clrbits_le32(CPU_RESET_PROT_STAT, SYS_RST_PROT_VCORE_M);
	setbits_le32(GCB_SOFT_RST, CHIP_SOFT_RST_M);
}

static char *part_id_string(u32 chip_id_reg)
{
	switch (FIELD_GET(PART_ID_M, chip_id_reg)) {
	case LAN9691VAO:
		return "LAN9691VAO";
	case LAN9692VAO:
		return "LAN9692VAO";
	case LAN9693VAO:
		return "LAN9693VAO";
	case LAN9694:
		return "LAN9694";
	case LAN9694TSN:
		return "LAN9694TSN";
	case LAN9694RED:
		return "LAN9694RED";
	case LAN9696:
		return "LAN9696";
	case LAN9696TSN:
		return "LAN9696TSN";
	case LAN9696RED:
		return "LAN9696RED";
	case LAN9698:
		return "LAN9698";
	case LAN9698TSN:
		return "LAN9698TSN";
	case LAN9698RED:
		return "LAN9698RED";
	default:
		return "Unknown ID";
	}
}

int print_cpuinfo(void)
{
	printf("CPU:   %s\n", part_id_string(readl(GCB_CHIP_ID)));

	return 0;
}
