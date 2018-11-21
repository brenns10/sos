/*
 * Memory Management Unit configuration
 *
 * In the qemu virt machine, physical memory begins at 0x40000000 (1GB).
 *
 * We assume that there are 3GB of physical memory, thus we have the entire
 * remaining physical address space. Further, to simplify things, the operating
 * system will have an identity mapping, and 1GB of memory dedicated to it.
 *
 * Applications are given the virtual memory section starting from 0x80000000.
 * The mapping there may not be identity, since the goal is to have multiple
 * applications having isolated memory mappings.
 *
 * MMU is described in ARM Architecture Reference section B4. For now, we'll use
 * first-layer only, which maps the most significant 12 bits into a table, with
 * each entry taking up 4 bytes. The table must be 4 * 2^12 bits long, or 16KB.
 * We can further subdivide into second-layer mappings, but I'm not implementing
 * that yet.
 *
 * The first 2GB (= 2048 entries, = 8192 bytes) therefore are identity mappings,
 * and we'll use the 16MB super-section format to hold them. The remaining 2GB
 * we're going to leave un-mapped, using them some other time.
 *
 * Notes:
 * - Ensure subpages enabled (I think?) CP15 reg 1 XP=1
 * - Ensure S and R are 0 in CP15 register 1
 */
#include <stdint.h>

/* FL_: first level constants */
#define FL_APX (1 << 15)
#define FL_AP1 (1 << 11)
#define FL_AP0 (1 << 10)

/* should be used whenever doing a section or supersection */
#define FL_SECTION (2)
#define FL_COARSE  (1)
#define FL_UNMAPPED (0)

/* should be used for supersection */
#define FL_SUPERSECTION (1 << 18)

/* eXecute Never */
#define FL_XN (1 << 4)

/* not global */
#define FL_NG (1 << 17)

/* shared */
#define FL_S (1 << 16)

/* TEX+C+B: defines caching, strong ordering. It's probably safe to use 0 */

/* Privileged or User (P / U) + NA=no access, RO=readonly, RW=readwrite */
#define FL_PNA_UNA 0                          /* apx 0, ap 00 */
#define FL_PRW_UNA (FL_AP0)                   /* apx 0, ap 01 */
#define FL_PRW_URO (FL_AP1)                   /* apx 0, ap 10 */
#define FL_PRW_URW (FL_AP1 | FL_AP0)          /* apx 0, ap 11 */
#define FL_PRO_UNA (FL_APX | FL_AP0)          /* apx 1, ap 01 */
#define FL_PRO_URO (FL_APX | FL_AP1)          /* apx 1, ap 10 */

void init_identity_mapping(uint32_t *base, uint32_t len, uint32_t attrs)
{
	uint32_t i;
	for (i = 0; i < len; i++) {
		base[i] = (i << 20) | attrs;
	}
}

void init_page_tables(uint32_t *base)
{
	/* First 2GB is identity mapping, not accessible to user, but accessible
	 * to SVC mode. */
	init_identity_mapping(base, 2048,
		/* Just use regular sections, no supersection */
		FL_SECTION |
		/* Privileged RW, User NA */
		FL_PRW_UNA |
		/* Global and shared, no caching, strong ordering */
		0
	);

	/* Next 20B is unmapped. */
	init_identity_mapping(base + 2048, 2048, FL_UNMAPPED);
}
