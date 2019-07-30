/**
 * ARM Generic Interrupt Controller Driver
 */
#include "gic.h"
#include "kernel.h"

#define GIC_IF_BASE 0x08010000
#define GIC_DIST_BASE 0x08000000

static gic_distributor_registers *gic_dregs;
static gic_cpu_interface_registers *gic_ifregs;

void gic_init(void)
{
	gic_dregs = (gic_distributor_registers *)alloc_pages(
	        kern_virt_allocator, 0x1000, 0);
	gic_ifregs = (gic_cpu_interface_registers *)alloc_pages(
	        kern_virt_allocator, 0x1000, 0);
	kmem_map_pages((uint32_t)gic_dregs, GIC_DIST_BASE, 0x1000, PRW_UNA);
	kmem_map_pages((uint32_t)gic_ifregs, GIC_IF_BASE, 0x1000, PRW_UNA);

	WRITE32(gic_ifregs->CCPMR,
	        0xFFFFu); /* enable all interrupt priorities */
	WRITE32(gic_ifregs->CCTLR,
	        3u); /* enable interrupt forwarding to this cpu */
	WRITE32(gic_dregs->DCTLR, 3u); /* enable distributor */
}

void gic_enable_interrupt(uint8_t int_id)
{
	uint8_t reg = int_id / 32;
	uint8_t bit = int_id % 32;

	/* Write 1 to the set-enable bit for this interrupt */
	uint32_t reg_val = gic_dregs->DISENABLER[reg];
	reg_val |= (1u << bit);
	WRITE32(gic_dregs->DISENABLER[reg], reg_val);

	/* Enable forwarding this interrupt to cpu 0 */
	reg = int_id / 4;
	bit = (int_id % 4) * 8;
	reg_val = gic_dregs->DITARGETSR[reg];
	reg_val |= (1u << bit);
	WRITE32(gic_dregs->DITARGETSR[reg], reg_val);
}

uint32_t gic_interrupt_acknowledge(void) { return gic_ifregs->CIAR; }

void gic_end_interrupt(uint8_t int_id) { WRITE32(gic_ifregs->CEOIR, int_id); }
