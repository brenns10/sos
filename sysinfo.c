#include "lib.h"

/**
 * A handy macro that loads coprocessor info into a variable. Much nicer than
 * having to write an assembly function to do it.
 */
#define get_cpreg(dst, CRn, op1, CRm, op2) \
	__asm__ __volatile__ ( \
		"mrc p15, " #op1 ", %[rd], " #CRn ", " #CRm ", " #op2 \
		: [rd] "=r" (dst) \
		: \
		: \
		)

#define sysinfo_entry(CRn, op1, CRm, op2, fstr) \
	do { \
		get_cpreg(ra, CRn, op1, CRm, op2); \
		printf(fstr ": %x\n", ra); \
	} while (0)

#define sysinfo_bit(name, reg, mask) \
	printf("%s: %s\n", name, reg & (mask) ? "yes" : "no")


void sysinfo(void)
{
	uint32_t ra;
	get_cpreg(ra, c0, 0, c0, 0);
	sysinfo_bit("Separate TLB", ra, 1);

	get_cpreg(ra, c1, 0, c0, 0);
	sysinfo_bit("MMU Enable", ra, 1);
	sysinfo_bit("Alignment fault", ra, 1<<1);
	sysinfo_bit("Write buffer", ra, 1<<3);
	sysinfo_bit("System protection", ra, 1<<8);
	sysinfo_bit("ROM protection", ra, 1<<9);
	sysinfo_bit("Extended page tables", ra, 1<<23);
	sysinfo_bit("Exception endian", ra, 1<<25);

	sysinfo_entry(c2, 0, c0, 0, "TTBR0");
	sysinfo_entry(c2, 0, c0, 1, "TTBR1");
	sysinfo_entry(c2, 0, c0, 2, "Translation table base control");

	sysinfo_entry(c3, 0, c0, 0, "Domain access control");
}
