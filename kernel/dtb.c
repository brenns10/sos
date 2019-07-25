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
 * Track the location of device tree sections in memory
 */
struct fdt_info {
	struct fdt_header *hdr; /* decoded */
	struct fdt_reserve_entry *rsv;
	void *tok;
	void *str;
} info;

struct dt_path {
	char *path[16];
	unsigned int len;
};

enum dt_fmt {
	FMT_UNKNOWN = 0, /* prop-encoded-array is included here */
	/* these are described in the spec as types */
	FMT_EMPTY,
	FMT_U32,
	FMT_U64,
	FMT_STRING,
	FMT_PHANDLE,
	FMT_STRINGLIST,
	/* these are described in the spec, but for particular props */
	FMT_REG,
	FMT_RANGES,
};

/*
 * Information about address cells and size cells, which must be kept as
 * bookkeeping.
 */
struct dt_reg_info {
	uint32_t address_cells;
	uint32_t size_cells;
	uint32_t interrupt_cells;
};

/*
 * Info about the current iteration over the device tree
 */
struct dtb_iter {
	/*
	 * Path of node that was just started, or ended, or for which this
	 * attribute applies.
	 */
	struct dt_path path;
	/*
	 * All address-cells / size-cells along the path
	 */
	struct dt_reg_info reginfo[16];
	/*
	 * A pointer to the current token in the tree.
	 */
	uint32_t *tok;
	/*
	 * The type of the current token.
	 */
	uint32_t typ;
	/*
	 * When an property is being visited (i.e. when typ == FDT_PROP), this
	 * contains the name of the property. Otherwise, this is NULL.
	 */
	char *prop;
	/*
	 * When a property is being visited, this contains the length of that
	 * section
	 */
	uint32_t proplen;
	/*
	 * When a property is being visited, this contains the address of the
	 * property value. (which is simply tok + 12 bytes, but this simplifies
	 * things)
	 */
	void *propaddr;
};


static void print_attr_unknown(const struct dtb_iter *, void *);
static void print_attr_empty(const struct dtb_iter *, void *);
static void print_attr_u32(const struct dtb_iter *, void *);
static void print_attr_u64(const struct dtb_iter *, void *);
static void print_attr_string(const struct dtb_iter *, void *);
static void print_attr_stringlist(const struct dtb_iter *, void *);
static void print_attr_reg(const struct dtb_iter *, void *);
static void print_attr_ranges(const struct dtb_iter *, void *);

typedef void (*dt_prop_printer)(const struct dtb_iter*, void*);
dt_prop_printer PRINTERS[] = {
	print_attr_unknown,
	print_attr_empty,
	print_attr_u32,
	print_attr_u64,
	print_attr_string,
	print_attr_u32,
	print_attr_stringlist,
	print_attr_reg,
	print_attr_ranges,
};

struct dt_prop_fmt {
	char *prop;
	enum dt_fmt fmt;
};

struct dt_prop_fmt STD_PROPS[] = {
	{.prop="compatible", .fmt=FMT_STRINGLIST},
	{.prop="model", .fmt=FMT_STRING},
	{.prop="phandle", .fmt=FMT_U32},
	{.prop="status", .fmt=FMT_STRING},
	{.prop="#address-cells", .fmt=FMT_U32},
	{.prop="#size-cells", .fmt=FMT_U32},
	{.prop="#interrupt-cells", .fmt=FMT_U32},
	{.prop="reg", .fmt=FMT_REG},
	{.prop="virtual-reg", .fmt=FMT_U32},
	{.prop="ranges", .fmt=FMT_RANGES},
	{.prop="dma-ranges", .fmt=FMT_RANGES},
	{.prop="name", .fmt=FMT_STRING},
	{.prop="device_type", .fmt=FMT_STRING},
	{.prop="interrupt-parent", .fmt=FMT_PHANDLE},
};

/* Bits determining when a callback for device tree iteration is executed */
#define DT_ITER_BEGIN_NODE 0x01
#define DT_ITER_END_NODE   0x02
#define DT_ITER_PROP       0x04

/**
 * Parse a device tree node path. Return 0 on success, non-0 on failure;
 */
static int dt_path_parse(struct dt_path *path, char *str)
{
	unsigned int i;
	unsigned int start;

	/* Special case for root */
	if (str[0] != '/')
		return 1;
	path->path[0] = "";
	path->len = 1;

	start = 1;
	for (i = 1; str[i]; i++) {
		if (str[i] == '/') {
			str[i] = '\0';
			path->path[path->len++] = &str[start];
			start = i+1;
		}
	}

	if (start != i) {
		path->path[path->len++] = &str[start];
	}
	return 0;
}

/**
 * Compare device tree paths.
 */
static bool dt_path_cmp(const struct dt_path *lhs, const struct dt_path *rhs)
{
	unsigned int i;

	if (lhs->len != rhs->len)
		return false;

	for (i = 0; i < lhs->len; i++) {
		if (strcmp(lhs->path[i], rhs->path[i]) != 0) {
			return false;
		}
	}
	return true;
}

/*
 * Return true when haystack starts with pfx.
 */
static bool dt_path_prefix(const struct dt_path *haystack, const struct dt_path *pfx)
{
	struct dt_path newhs = *haystack;
	newhs.len = pfx->len;
	return dt_path_cmp(&newhs, pfx);
}

static void dt_path_print(const struct dt_path *path)
{
	unsigned int i;
	if (path->len == 1) {
		puts("/");
		return;
	}

	for (i = 1; i < path->len; i++) {
		puts("/");
		puts(path->path[i]);
	}
}

static enum dt_fmt dt_lookup_fmt(const char *propname)
{
	unsigned int i;
	for (i = 0; i < nelem(STD_PROPS); i++) {
		if (strcmp(propname, STD_PROPS[i].prop) == 0)
			return STD_PROPS[i].fmt;
	}
	return FMT_UNKNOWN;
}

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

static void print_attr_unknown(const struct dtb_iter *iter, void *data)
{
	printf("%s: (data in unknown format)\n", iter->prop);
}

static void print_attr_empty(const struct dtb_iter *iter, void *data)
{
	printf("%s\n", iter->prop);
}

static void print_attr_u32(const struct dtb_iter *iter, void *data)
{
	printf("%s: 0x%x\n", iter->prop, be2host(*(uint32_t*)iter->propaddr));
}

static void print_attr_u64(const struct dtb_iter *iter, void *data)
{
	printf("%s: 0x%x %x\n", iter->prop,
		be2host(*(uint32_t*)iter->propaddr),
		be2host(*(uint32_t*)(iter->propaddr+4)));
}

static void print_attr_string(const struct dtb_iter *iter, void *data)
{
	printf("%s: \"%s\"\n", iter->prop, (char*)iter->propaddr);
}

static void print_attr_stringlist(const struct dtb_iter *iter, void *data)
{
	unsigned int len = 0;
	char *str = iter->propaddr;
	printf("%s: [", iter->prop);
	while (len < iter->proplen) {
		printf("\"%s\" ", str);
		len += strlen(str) + 1;
		str = iter->propaddr + len;
	}
	puts("]\n");
}

static void print_attr_reg(const struct dtb_iter *iter, void *data)
{
	uint32_t *reg = iter->propaddr;
	int remain = (int)iter->proplen;
	uint32_t address_cells = iter->reginfo[iter->path.len-2].address_cells;
	uint32_t size_cells = iter->reginfo[iter->path.len-2].size_cells;
	uint32_t i;
	/*
	 * Reg is arbitrarily long list of 2-tuples containing (address, size)
	 * of the memory mapped region assigned. "address" has size
	 * #address-cells from parent node, and "size" has size #size-cells from
	 * parent node, both in terms of u32s.
	 */
	printf("%s: ", iter->prop);
	while (remain > 0) {
		puts("<0x");
		for (i = 0; i < address_cells; i++)
			printf("%x ", reg[i]);
		reg += address_cells;
		remain -= address_cells * 4;
		puts(", 0x");
		for (i = 0; i < size_cells; i++)
			printf("%x ", reg[i]);
		puts("> ");
		reg += size_cells;
		remain -= size_cells * 4;
	}
	puts("\n");
}

static void print_attr_ranges(const struct dtb_iter *iter, void *data)
{
	uint32_t *reg = iter->propaddr;
	int remain = (int)iter->proplen;
	uint32_t address_cells = iter->reginfo[iter->path.len-1].address_cells;
	uint32_t size_cells = iter->reginfo[iter->path.len-1].size_cells;
	uint32_t i;
	/*
	 * Ranges is arbitrarily long list of 3-tuples containing
	 * (child address, parent address, size)
	 * of the memory mapped region assigned. "address" has size
	 * #address-cells from current node, and "size" has size #size-cells from
	 * current node, both in terms of u32s.
	 */

	printf("%s: ", iter->prop);
	while (remain > 0) {
		puts("<0x");
		for (i = 0; i < address_cells; i++)
			printf("%x ", reg[i]);
		reg += address_cells;
		remain -= address_cells * 4;

		puts(", 0x");
		for (i = 0; i < address_cells; i++)
			printf("%x ", reg[i]);
		reg += address_cells;
		remain -= address_cells * 4;

		puts(", 0x");
		for (i = 0; i < size_cells; i++)
			printf("%x ", reg[i]);
		puts("> ");
		reg += size_cells;
		remain -= size_cells * 4;
	}
	puts("\n");
}

void dtb_mem_reserved(void)
{
	struct fdt_reserve_entry *entry;

	printf("+ reserved memory regions at 0x%x\n", info.rsv);
	for (entry = info.rsv; entry->addr_le || entry->size_le; entry++)
		printf("  addr=0x%x, size=0x%x\n", be2host(entry->addr_le),
				be2host(entry->size_le));
}

/**
 * Iterate over the device tree! This is a configurable function which executes
 * your callback at whichever point in the iteration, and can terminate at
 * whatever point you want.
 *
 * Args:
 *   cb_flags: Specify what tokens you'd like your callback to get called at.
 *     Uses the bit constants DTB_ITER_*.
 *   cb: A function which is called at particular tokens during the iteration.
 *     If this returns true, the iteration returns early. The first argument to
 *     this callback is a struct dtb_iter *, which MUST NOT be modified. See the
 *     difinition of struct dtb_iter for all the juicy details on what it
 *     contains. The second argument to the callback is an arbitrary "data"
 *     which the caller defines.
 *   data: Some data to give to the callback
 *
 * Returns: void!
 */
void dtb_iter(unsigned int cb_flags, bool (*cb)(const struct dtb_iter *, void *), void *data)
{
	char *str;

	struct dtb_iter iter;
	iter.path.len = 0;
	iter.tok = info.tok;

	while (1) {
		iter.typ = be2host(*iter.tok);
		switch (iter.typ) {
			case FDT_NOP:
				iter.tok++;
				break;
			case FDT_BEGIN_NODE:
				str = (char *)iter.tok + 4;
				iter.path.path[iter.path.len] = str;
				/* defaults from section 2.3.5 of DTS */
				iter.reginfo[iter.path.len].address_cells = 2;
				iter.reginfo[iter.path.len].size_cells = 1;
				/* no idea what the default is, 2 is frequently
				 * used in the DTS */
				iter.reginfo[iter.path.len].interrupt_cells = 2;
				iter.path.len++;
				if (cb_flags & DT_ITER_BEGIN_NODE)
					if (cb(&iter, data))
						return;
				iter.tok = (uint32_t*)align((uint32_t)str + strlen(str) + 1, 2);
				break;
			case FDT_END_NODE:
				if (cb_flags & DT_ITER_END_NODE)
					if (cb(&iter, data))
						return;
				iter.path.len--;
				iter.tok++;
				break;
			case FDT_PROP:
				iter.prop = info.str + be2host(iter.tok[2]);
				iter.proplen = be2host(iter.tok[1]);
				iter.propaddr = (void*)iter.tok + 12;

				/* regardless of callbacks, we must track the
				 * #address-cells and #size-cells attributes as
				 * we parse, in order to be able to make sense
				 * of child attributes like regs
				 */
				if (strcmp(iter.prop, "#address-cells") == 0) {
					iter.reginfo[iter.path.len-1].address_cells = (
						be2host(iter.tok[3]));
				} else if (strcmp(iter.prop, "#size-cells") == 0) {
					iter.reginfo[iter.path.len-1].size_cells = (
						be2host(iter.tok[3]));
				} else if (strcmp(iter.prop, "#interrupt-cells") == 0) {
					iter.reginfo[iter.path.len-1].interrupt_cells = (
						be2host(iter.tok[3]));
				}


				if (cb_flags & DT_ITER_PROP)
					if (cb(&iter, data))
						return;
				iter.prop = NULL;
				iter.propaddr = NULL;
				iter.tok = (uint32_t*)align((uint32_t)iter.tok + 12 + iter.proplen, 2);
				iter.proplen = 0;
				break;
			case FDT_END:
				return;
			default:
				printf("unrecognized token 0x%x\n", iter.typ);
				return;
		}
	}
}

static bool dtb_ls_cb(const struct dtb_iter *iter, void *data)
{
	struct dt_path *path = data;

	if (!dt_path_prefix(&iter->path, path)) {
		return false;
	}

	if (iter->path.len > path->len + 1)
		return false;

	dt_path_print(&iter->path);
	puts("\n");
	return false;
}

int cmd_dtb_ls(int argc, char **argv)
{
	struct dt_path path;
	if (argc != 2) {
		puts("usage: dtb-ls DTBPATH\n");
		return 1;
	}

	if (dt_path_parse(&path, argv[1]) != 0) {
		puts("error: bad dtb path\n");
		return 1;
	}

	dtb_iter(DT_ITER_BEGIN_NODE, dtb_ls_cb, &path);
}

static bool dtb_prop_cb(const struct dtb_iter *iter, void *data)
{
	struct dt_path *path = data;
	enum dt_fmt fmt;

	if (!dt_path_cmp(&iter->path, path)) {
		return false;
	}

	fmt = dt_lookup_fmt(iter->prop);
	PRINTERS[fmt](iter, data);
	return false;
}

int cmd_dtb_prop(int argc, char **argv)
{
	struct dt_path path;
	if (argc != 2) {
		puts("usage: dtb-prop DTBPATH\n");
		return 1;
	}

	if (dt_path_parse(&path, argv[1]) != 0) {
		puts("error: bad dtb path\n");
		return 1;
	}

	dtb_iter(DT_ITER_PROP, dtb_prop_cb, &path);
	return 0;
}

static char *spaces(uint32_t n)
{
	static char sp[] = "                ";
	if (n > sizeof(sp) - 1)
		n = sizeof(sp) - 1;
	return sp + (sizeof(sp) - n - 1);
}

static bool dtb_dump_cb(const struct dtb_iter *iter, void *data)
{
	enum dt_fmt fmt;
	switch (iter->typ) {
	case FDT_BEGIN_NODE:
		printf("%s%s {\n", spaces(iter->path.len-1),
			iter->path.path[iter->path.len-1]);
		break;
	case FDT_END_NODE:
		printf("%s}\n", spaces(iter->path.len - 1));
		break;
	case FDT_PROP:
		fmt = dt_lookup_fmt(iter->prop);
		puts(spaces(iter->path.len));
		PRINTERS[fmt](iter, data);
	}
	return false;
}

int cmd_dtb_dump(int argc, char **argv)
{
	if (argc != 1) {
		puts("usage: dtb-dump\n");
		return 1;
	}

	dtb_iter(DT_ITER_BEGIN_NODE|DT_ITER_END_NODE|DT_ITER_PROP, dtb_dump_cb, NULL);
	return 0;
}

/*
 * Initialize "dtb" module, given the location of the flattened device tree in
 * memory (physical address). This must be called only once, and it must be
 * called before the dtb_iter() method can be used.
 */
void dtb_init(uint32_t phys)
{
	uint32_t virt = alloc_pages(kern_virt_allocator, 0x4000, 0);
	kmem_map_pages((uint32_t)virt, phys, 0x4000, PRW_UNA);

	info.hdr = (struct fdt_header *) virt;
	info.rsv = (void*) (virt + be2host(info.hdr->off_mem_rsvmap));
	info.tok = (void*) (virt + be2host(info.hdr->off_dt_struct));
	info.str = (void*) (virt + be2host(info.hdr->off_dt_strings));
}
