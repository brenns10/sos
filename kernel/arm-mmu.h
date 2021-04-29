#pragma once

/*
 * arm-mmu.h: a private header that really should only be included by kmem.c.
 */

/*
 * First Level Descriptors
 *
 * The FLD__ constants here apply to section address descriptors. Page table
 * address descriptors do not have these fields.
 */
#define FLD__AP2  (1 << 15)
#define FLD__AP1  (1 << 11)
#define FLD__AP0  (1 << 10)
#define FLD__C    (1 << 3)
#define FLD__B    (1 << 2)
#define FLD__TEX0 (1 << 12)
#define FLD__TEX1 (1 << 13)
#define FLD__TEX2 (1 << 14)
#define FLD__S    (1 << 16)

#define FLD_UNMAPPED 0x00
#define FLD_COARSE   0x01
#define FLD_SECTION  0x02
#define FLD_MASK 0x03

#define FLD_PAGE_TABLE(fld)   ((fld) & ~0x3FF)
#define FLD_SECTION_ADDR(fld) ((fld) & 0xFFF00000)

// Given a virtual address, return the index into first level table
#define fld_idx(x) (((uint32_t) x) >> 20)

/*
 * Second Level Descriptors
 *
 * These apply to second level "small page" descriptors.
 */
#define SLD__AP2  (1 << 9)
#define SLD__AP1  (1 << 5)
#define SLD__AP0  (1 << 4)
#define SLD__C    (1 << 3)
#define SLD__B    (1 << 2)
#define SLD__TEX0 (1 << 6)
#define SLD__TEX1 (1 << 7)
#define SLD__TEX2 (1 << 8)
#define SLD__S    (1 << 10)
#define SLD_NG    (1 << 11)

#define SLD_UNMAPPED 0x00
#define SLD_LARGE    0x01
#define SLD_SMALL    0x02

#define SLD_MASK 0x03
#define SLD_ADDR(sld) ((sld) & 0xFFFFF000)

// Given virtual address, return index into second level table
#define sld_idx(x) ((((uint32_t) x) >> 12) & 0xFF)


/*
 * Permissions (combinations of the above FLD / SLD constants)
 */
#define SLD_PRW_UNA       (SLD__AP0) /* AP=0b001 */
#define SLD_PRW_URO       (SLD__AP1) /* AP=0b010 */
#define SLD_PRW_URW       (SLD__AP1 | SLD__AP0) /* AP=0b011 */
#define SLD_PRO_UNA       (SLD__AP2 | SLD__AP0) /* AP=0b101 */
#define SLD_PRO_URO       (SLD__AP2 | SLD__AP1) /* AP=0b110 */
#define SLD_EXECUTE_NEVER 0x01

#define FLD_PRW_UNA       (FLD__AP0) /* AP=0b001 */
#define FLD_PRW_URO       (FLD__AP1) /* AP=0b010 */
#define FLD_PRW_URW       (FLD__AP1 | FLD__AP0) /* AP=0b011 */
#define FLD_PRO_UNA       (FLD__AP2 | FLD__AP0) /* AP=0b101 */
#define FLD_PRO_URO       (FLD__AP2 | FLD__AP1) /* AP=0b110 */

/* Memory attributes: Choose either:
 * - DEVICE_SHAREABLE
 * - DEVICE_NONSHAREABLE
 * - NORMAL_SHAREABLE
 * - NORMAL_NONSHAREABLE
 *
 * Normal memory can further select cache attributes.
 */
#define SLD_DEVICE_SHAREABLE    (0)
#define SLD_DEVICE_NONSHAREABLE (0)
#define SLD_NORMAL_SHAREABLE    (SLD__TEX0 | SLD__C | SLD__B | SLD__S)
#define SLD_NORMAL_NONSHAREABLE (SLD__TEX0 | SLD__C | SLD__B)

#define FLD_DEVICE_SHAREABLE    (0)
#define FLD_DEVICE_NONSHAREABLE (0)
#define FLD_NORMAL_SHAREABLE    (FLD__TEX0 | FLD__C | FLD__B | FLD__S)
#define FLD_NORMAL_NONSHAREABLE (FLD__TEX0 | FLD__C | FLD__B)

#define SLD_CACHE_INNER_NC      (0)
#define SLD_CACHE_INNER_WBWA    (SLD__TEX0)
#define SLD_CACHE_INNER_WT      (SLD__TEX1)
#define SLD_CACHE_INNER_WB      (SLD__TEX1 | SLD__TEX0)
#define SLD_CACHE_OUTER_NC      (0)
#define SLD_CACHE_OUTER_WBWA    (SLD__B)
#define SLD_CACHE_OUTER_WT      (SLD__C)
#define SLD_CACHE_OUTER_WB      (SLD__C | SLD__B)

// Flags for different types of memory
#define KMEM_DEFAULT (FLD_NORMAL_SHAREABLE | FLD_PRW_UNA)
#define UMEM_FLAGS_RW (SLD_NORMAL_SHAREABLE | SLD_PRW_URW | SLD_NG)
#define UMEM_FLAGS_RO (SLD_NORMAL_SHAREABLE | SLD_PRW_URW | SLD_NG)
#define PERIPH_DEFAULT (SLD_PRW_UNA | SLD_EXECUTE_NEVER | SLD_DEVICE_NONSHAREABLE)
