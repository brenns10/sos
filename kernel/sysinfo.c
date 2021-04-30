/**
 * sysinfo.c: print bits and fields of system registers
 */
#include "kernel.h"

#define sysinfo_entry(CRn, op1, CRm, op2, fstr)                                \
	do {                                                                   \
		get_cpreg(ra, CRn, op1, CRm, op2);                             \
		printf(fstr ": %x\n", ra);                                     \
	} while (0)

#define sysinfo_bit(name, reg, mask)                                           \
	printf("%s: %s\n", name, reg &(mask) ? "yes" : "no")

static struct field sctlr_fields[] = {
       FIELD_BIT("DSSBS", 31), FIELD_BIT("TE", 30),  FIELD_BIT("AFE", 29),
       FIELD_BIT("TRE", 28),   FIELD_BIT("EE", 25),  FIELD_BIT("SPAN", 23),
       FIELD_BIT("UWXN", 20),  FIELD_BIT("WXN", 19), FIELD_BIT("nTWE", 18),
       FIELD_BIT("nTWI", 16),  FIELD_BIT("V", 13),   FIELD_BIT("I", 12),
       FIELD_BIT("SED", 8),    FIELD_BIT("ITD", 7),  FIELD_BIT("CP15BEN", 5),
       FIELD_BIT("C", 2),      FIELD_BIT("A", 1),    FIELD_BIT("M", 0),
};

static struct field ttbcr_fields[] = {
       FIELD_BIT("EAE", 31),
       FIELD_BIT("PD1", 5),
       FIELD_BIT("PD0", 4),
       FIELD_3BIT("N", 0),
};

static struct field ttbr_fields[] = {
       FIELD_MASK("TTB", 0xFFFFFF80), FIELD_BIT("IRGN[0]", 6),
       FIELD_BIT("NOS", 5),           FIELD_2BIT("RGN", 3),
       FIELD_BIT("IMP", 2),           FIELD_BIT("S", 1),
       FIELD_BIT("IRGN[1]", 0),
};

void sysinfo(void)
{
	uint32_t ra;
	get_cpreg(ra, c0, 0, c0, 0);
	sysinfo_bit("Separate TLB", ra, 1);

	puts("SCTLR\n");
	dissect_fields(get_sctlr(), sctlr_fields, nelem(sctlr_fields));
	puts("TTBCR\n");
	dissect_fields(get_ttbcr(), ttbcr_fields, nelem(ttbcr_fields));
	puts("TTBR0\n");
	dissect_fields(get_ttbr0(), ttbr_fields, nelem(ttbr_fields));
	puts("TTBR1\n");
	dissect_fields(get_ttbr1(), ttbr_fields, nelem(ttbr_fields));
	printf("DACR: 0x%x\n", get_dacr());	get_cpreg(ra, c1, 0, c0, 0);
}
