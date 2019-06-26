/*
 * Device Tree Parsing
 */
#include "kernel.h"
#include "string.h"
#include "util.h"

/*
 * Token types.
 */
#define FDT_BEGIN_NODE (0x00000001)
#define FDT_END_NODE   (0x00000002)
#define FDT_PROP       (0x00000003)
#define FDT_NOP        (0x00000004)
#define FDT_END        (0x00000009)

/*
 * Header copied from device tree spec
 */
struct fdt_header {
	uint32_t magic;
	uint32_t totalsize;
	uint32_t off_dt_struct;
	uint32_t off_dt_strings;
	uint32_t off_mem_rsvmap;
	uint32_t version;
	uint32_t last_comp_version;
	uint32_t boot_cpuid_phys;
	uint32_t size_dt_strings;
	uint32_t size_dt_struct;
};

/*
 * Memory reservation entry
 */
struct fdt_reserve_entry {
	uint32_t addr_be;
	uint32_t addr_le;
	uint32_t size_be;
	uint32_t size_le;
};

/*
 * Big-endian sucks sometimes
 */
uint32_t be2host(uint32_t orig)
{
	return ((orig & 0xFF) << 24)
		| ((orig & 0xFF00) << 8)
		| ((orig & 0xFF0000) >> 8)
		| ((orig & 0xFF000000) >> 24);
}

struct fdt_info {
	struct fdt_header *hdr; /* decoded */
	struct fdt_reserve_entry *rsv;
	void *tok;
	void *str;
};

void dtb_mem_reserved(struct fdt_info *info)
{
	struct fdt_reserve_entry *entry;

	printf("+ reserved memory regions at 0x%x\n", info->rsv);
	for (entry = info->rsv; entry->addr_le || entry->size_le; entry++)
		printf("  addr=0x%x, size=0x%x\n", be2host(entry->addr_le),
				be2host(entry->size_le));
}

static char *spaces(uint32_t n)
{
	static char sp[] = "                ";

	if (n > sizeof(sp) - 1)
		n = sizeof(sp) - 1;

	return sp + (sizeof(sp) - n - 1);
}

void dtb_attr(char *name, uint32_t len, void *val)
{
}

void dtb_tokens(struct fdt_info *info)
{
	char *str;
	uint32_t *tok = info->tok;
	uint32_t nest = 0, len;

	while (1) {
		switch (be2host(*tok)) {
			case FDT_NOP:
				tok++;
				break;
			case FDT_BEGIN_NODE:
				str = (char *)tok + 4;
				printf("%snode \"%s\" {\n", spaces(nest), str);
				tok = (uint32_t*)align((uint32_t)str + strlen(str) + 1, 2);
				nest++;
				break;
			case FDT_END_NODE:
				nest--;
				printf("%s}\n", spaces(nest));
				tok++;
				break;
			case FDT_PROP:
				str = info->str + be2host(tok[2]);
				len = be2host(tok[1]);
				printf("%sattr \"%s\", len=%u\n",
					spaces(nest),
					info->str + be2host(tok[2]),
					len);
				tok = (uint32_t*)align((uint32_t)tok + 12 + be2host(tok[1]), 2);
				break;
			case FDT_END:
				return;
			default:
				printf("unrecognized token 0x%x\n", be2host(*tok));
				return;
		}
	}
}

void dtb_parse(uint32_t phys)
{
	struct fdt_info info;
	uint32_t virt = alloc_pages(kern_virt_allocator, 0x4000, 0);
	kmem_map_pages((uint32_t)virt, phys, 0x4000, PRW_UNA);

	info.hdr = (struct fdt_header *) virt;
	info.rsv = (void*) (virt + be2host(info.hdr->off_mem_rsvmap));
	info.tok = (void*) (virt + be2host(info.hdr->off_dt_struct));
	info.str = (void*) (virt + be2host(info.hdr->off_dt_strings));

	printf("fdt (dtb) at 0x%x\n", info.hdr);

	dtb_mem_reserved(&info);
	dtb_tokens(&info);
}
