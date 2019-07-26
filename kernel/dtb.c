/*
 * Device Tree Parsing
 */
#include "kernel.h"
#include "string.h"
#include "util.h"

/*********************
 * Flattened Device Tree Declarations
 */

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

/*********************
 * Internal Bookkeeping Declarations
 */

#define MAX_DEPTH 16
#define MAX_NODES 128

/*
 * A single global tracking the locations of all the device tree sections in
 * memory.
 */
struct fdt_info {
	struct fdt_header *hdr; /* decoded */
	struct fdt_reserve_entry *rsv;
	void *tok;
	void *str;
} info;

/*
 * An array of per-node data that gets filled out on initialization.
 */
struct dt_node {
	char *name;
	uint32_t *tok;
	struct dt_node *parent;
	struct dt_node *interrupt_parent;
	uint32_t address_cells;
	uint32_t size_cells;
	uint32_t interrupt_cells;
	uint32_t phandle;
	unsigned short depth;
} nodes[MAX_NODES];

/*
 * Global describing how many nodes there are total.
 */
unsigned int nodecount = 0;

/*
 * Mapping phandles to nodes
 */
struct {
	uint32_t phandle;
	struct dt_node *node;
} phandle_map[MAX_NODES];

/*
 * How many phandles mapped?
 */
unsigned int phandlecount = 0;

/*
 * Data structure encoding a "path" for a device tree node. Useful for
 * describing nodes and shell operations.
 */
struct dt_path {
	char *path[MAX_DEPTH];
	unsigned int len;
};

/*
 * "Formats" which an attribute may be in.
 */
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
	FMT_INTERRUPTS,
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
	/*
	 * Index into nodes array
	 */
	unsigned short nodeidx;
};


static void print_attr_unknown(const struct dtb_iter *, void *);
static void print_attr_empty(const struct dtb_iter *, void *);
static void print_attr_u32(const struct dtb_iter *, void *);
static void print_attr_u64(const struct dtb_iter *, void *);
static void print_attr_string(const struct dtb_iter *, void *);
static void print_attr_phandle(const struct dtb_iter *, void *);
static void print_attr_stringlist(const struct dtb_iter *, void *);
static void print_attr_reg(const struct dtb_iter *, void *);
static void print_attr_ranges(const struct dtb_iter *, void *);
static void print_attr_interrupts(const struct dtb_iter *, void *);

/*
 * Indexed by enum dt_fmt
 */
typedef void (*dt_prop_printer)(const struct dtb_iter*, void*);
dt_prop_printer PRINTERS[] = {
	print_attr_unknown,
	print_attr_empty,
	print_attr_u32,
	print_attr_u64,
	print_attr_string,
	print_attr_phandle,
	print_attr_stringlist,
	print_attr_reg,
	print_attr_ranges,
	print_attr_interrupts,
};

struct dt_prop_fmt {
	char *prop;
	enum dt_fmt fmt;
};

struct dt_propenc_field {
	int cells;
	bool phandle;
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
	{.prop="interrupt-controller", .fmt=FMT_EMPTY},
	{.prop="interrupts", .fmt=FMT_INTERRUPTS},
	{.prop="msi-parent", .fmt=FMT_PHANDLE},
};

/* Bits determining when a callback for device tree iteration is executed */
#define DT_ITER_BEGIN_NODE 0x01
#define DT_ITER_END_NODE   0x02
#define DT_ITER_PROP       0x04

/******************************
 * Path related functions!
 */

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

/************************
 * Phandle lookup
 */
static struct dt_node *lookup_phandle(uint32_t phandle)
{
	uint32_t i;
	for (i = 0; i < phandlecount; i++)
		if (phandle_map[i].phandle == phandle)
			return phandle_map[i].node;
	return NULL;
}

/************************
 * Functions related to attribute formats!
 */

/*
 * Return the format we have mapped a particular propname to.
 */
static enum dt_fmt dt_lookup_fmt(const char *propname, uint32_t proplen)
{
	unsigned int i;
	for (i = 0; i < nelem(STD_PROPS); i++) {
		if (strcmp(propname, STD_PROPS[i].prop) == 0)
			return STD_PROPS[i].fmt;
	}
	if (propname[0] == '#' && strsuffix(propname, "-cells"))
		return FMT_U32;
	if (proplen == 0)
		return FMT_EMPTY;
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
	printf("%s: (data in unknown format, len=%u)\n", iter->prop, iter->proplen);
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

static void print_attr_phandle(const struct dtb_iter *iter, void *data)
{
	uint32_t phandle = be2host(iter->tok[3]);
	struct dt_node *ref = lookup_phandle(phandle);
	if (ref) {
		printf("%s: <phandle &%s>\n", iter->prop, ref->name);
	} else {
		printf("%s: <phandle 0x%x>\n", iter->prop, phandle);
	}
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

static void print_attr_propenc(const struct dtb_iter *iter, struct dt_propenc_field fields[], int fcnt)
{
	uint32_t entry_size = 0, i, j, phandle, remain = iter->proplen;
	uint32_t *reg = iter->propaddr;
	struct dt_node *ref;

	printf("%s: ", iter->prop);

	for (i = 0; i < fcnt; i++) {
		entry_size += 4 * fields[i].cells;
		if (fields[i].phandle & fields[i].cells != 1)
			puts("malformed prop-encoded array, has phandle with len != 1\n");
	}

	printf("(len=%u/%u) ", iter->proplen, entry_size);

	if (iter->proplen % entry_size != 0) {
		printf("malformed prop-encoded array, len=%u, entry len=%u\n",
			iter->proplen, entry_size);
		return;
	}

	while (remain > 0) {
		puts("<");
		for (i = 0; i < fcnt; i++) {
			if (fields[i].phandle) {
				phandle = be2host(*reg);
				ref = lookup_phandle(phandle);
				if (ref) {
					printf("<phandle &%s>", ref->name);
				} else {
					printf("<phandle 0x%x>", phandle);
				}
				reg++;
				remain -= 4;
			} else {
				puts("0x");
				for (j = 0; j < fields[i].cells; j++) {
					printf("%x ", be2host(*reg));
					reg++;
					remain -= 4;
				}
			}
			puts("| ");
		}
		puts("> ");
	}
	puts("\n");
}

static void print_attr_reg(const struct dtb_iter *iter, void *data)
{
	unsigned short nodeidx = iter->nodeidx;
	struct dt_propenc_field fields[] = {
		{.cells=nodes[nodeidx].parent->address_cells, .phandle=false},
		{.cells=nodes[nodeidx].parent->size_cells, .phandle=false},
	};
	print_attr_propenc(iter, fields, nelem(fields));
}

static void print_attr_ranges(const struct dtb_iter *iter, void *data)
{
	unsigned short nodeidx = iter->nodeidx;
	struct dt_propenc_field fields[] = {
		{.cells=nodes[nodeidx].address_cells, .phandle=false},
		{.cells=nodes[nodeidx].parent->address_cells, .phandle=false},
		{.cells=nodes[nodeidx].size_cells, .phandle=false},
	};
	print_attr_propenc(iter, fields, nelem(fields));
}

static void print_attr_interrupts(const struct dtb_iter *iter, void *data)
{
	unsigned short nodeidx = iter->nodeidx;
	struct dt_node *node = nodes[nodeidx].parent;
	uint32_t interrupt_cells = 0;

	while (node && node->interrupt_cells == 0)
		node = node->interrupt_parent;
	if (node)
		interrupt_cells = node->interrupt_cells;

	struct dt_propenc_field fields[] = {
		{.cells=interrupt_cells, .phandle=false},
	};
	print_attr_propenc(iter, fields, nelem(fields));
}

/****************************
 * DTB Parsing!
 */

/*
 * Read the mem_reserved section and print info out
 */
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
	iter.nodeidx = 0;
	bool begin = true;

	while (1) {
		iter.typ = be2host(*iter.tok);
		switch (iter.typ) {
		case FDT_NOP:
			iter.tok++;
			break;
		case FDT_BEGIN_NODE:
			str = (char *)iter.tok + 4;
			iter.path.path[iter.path.len] = str;
			iter.path.len++;
			iter.nodeidx = begin ? 0 : (iter.nodeidx+1);
			begin = false;
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

/***************************
 * Device tree "commands" for the shell
 */

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

	fmt = dt_lookup_fmt(iter->prop, iter->proplen);
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

static char *indent(uint32_t n)
{
	static char sp[] = "                                               ";
	n *= 4;
	if (n > sizeof(sp) - 1)
		n = sizeof(sp) - 1;
	return sp + (sizeof(sp) - n - 1);
}

static bool dtb_dump_cb(const struct dtb_iter *iter, void *data)
{
	enum dt_fmt fmt;
	int nodeidx = iter->path.len - 1;
	switch (iter->typ) {
	case FDT_BEGIN_NODE:
		printf("%s%s {\n", indent(nodeidx),
			iter->path.path[nodeidx]);
		break;
	case FDT_END_NODE:
		printf("%s}\n", indent(nodeidx));
		break;
	case FDT_PROP:
		fmt = dt_lookup_fmt(iter->prop, iter->proplen);
		puts(indent(nodeidx + 1));
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

/*****************************
 * Initializations for the device tree "module"
 */

/*
 * This callback constructs tracking information about each node.
 */
static bool dtb_init_cb(const struct dtb_iter *iter, void *data)
{
	int i;
	unsigned short nodeidx = iter->nodeidx;
	switch (iter->typ) {
	case FDT_BEGIN_NODE:
		nodes[nodeidx].name = iter->path.path[iter->path.len-1];
		nodes[nodeidx].tok = iter->tok;
		nodes[nodeidx].depth = iter->path.len;
		nodes[nodeidx].address_cells = 2;
		nodes[nodeidx].size_cells = 1;
		nodes[nodeidx].interrupt_cells = 0;
		nodes[nodeidx].phandle = 0;
		if (nodeidx != 0) {
			i = nodeidx - 1;
			while (i >= 0 && nodes[i].depth != nodes[nodeidx].depth-1)
				i--;
			nodes[nodeidx].parent = &nodes[i];
		} else {
			nodes[nodeidx].parent = &nodes[nodeidx];
		}
		nodes[nodeidx].interrupt_parent = nodes[nodeidx].parent;
		break;
	case FDT_PROP:
		if (strcmp(iter->prop, "#address-cells") == 0) {
			nodes[nodeidx].address_cells = be2host(iter->tok[3]);
		} else if (strcmp(iter->prop, "#size-cells") == 0) {
			nodes[nodeidx].size_cells = be2host(iter->tok[3]);
		} else if (strcmp(iter->prop, "#interrupt-cells") == 0) {
			nodes[nodeidx].interrupt_cells = be2host(iter->tok[3]);
		} else if (strcmp(iter->prop, "phandle") == 0) {
			nodes[nodeidx].phandle = be2host(iter->tok[3]);
			phandle_map[phandlecount].phandle = nodes[nodeidx].phandle;
			phandle_map[phandlecount].node = &nodes[nodeidx];
			phandlecount++;
		}
		break;
	case FDT_END_NODE:
		nodecount = nodeidx + 1;
		break;
	}
	return false;
}

/*
 * Initialize "interrupt_parent" fields given our newly built phandle mapping.
 */
bool dtb_init_interrupt_cb(const struct dtb_iter *iter, void *data)
{
	unsigned short nodeidx = iter->nodeidx;
	uint32_t phandle;
	struct dt_node *parent;
	if (strcmp(iter->prop, "interrupt-parent") == 0) {
		phandle = be2host(iter->tok[3]);
		parent = lookup_phandle(phandle);
		if (parent) {
			nodes[nodeidx].interrupt_parent = parent;
		} else {
			puts("dtb: error: for node ");
			dt_path_print(&iter->path);
			printf(", interrupt-parent specifies unknown phandle 0x%x\n", phandle);
		}
	}
	return false;
}

/*
 * Initialize "dtb" module, given the location of the flattened device tree in
 * memory (physical address). This must be called only once, and it must be
 * called before the dtb_iter() method can be used.
 */
void dtb_init(uint32_t phys)
{
	uint32_t i;
	uint32_t virt = alloc_pages(kern_virt_allocator, 0x4000, 0);
	kmem_map_pages((uint32_t)virt, phys, 0x4000, PRW_UNA);

	info.hdr = (struct fdt_header *) virt;
	info.rsv = (void*) (virt + be2host(info.hdr->off_mem_rsvmap));
	info.tok = (void*) (virt + be2host(info.hdr->off_dt_struct));
	info.str = (void*) (virt + be2host(info.hdr->off_dt_strings));

	dtb_iter(DT_ITER_BEGIN_NODE|DT_ITER_END_NODE|DT_ITER_PROP, dtb_init_cb, NULL);
	dtb_iter(DT_ITER_PROP, dtb_init_interrupt_cb, NULL);
}
