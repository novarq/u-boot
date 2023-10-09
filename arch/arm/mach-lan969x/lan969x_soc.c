#include <asm/arch/soc.h>
#include <linux/arm-smccc.h>

#define SIP_SVC_UID             0x8200ff01
#define SIP_SVC_VERSION         0x8200ff02
#define SIP_SVC_GET_BOOTSRC	0x8200ff09
#define SIP_SVC_GET_DDR_SIZE	0x8200ff0a
#define SIP_SVC_GET_BOARD_NO	0x8200ff0b
#define SIP_SVC_SRAM_INFO       0x8200ff0d

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
