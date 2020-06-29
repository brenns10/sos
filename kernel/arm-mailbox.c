/*
 * Driver of the mbox communication system, primarily used for Raspberry Pi.
 */
#include "arm-mailbox.h"
#include "kernel.h"
#include <stdbool.h>
#include <stdint.h>

struct arm_mbox_ptag {
	uint32_t tagid;
	uint32_t size;
	uint32_t code;
	uint8_t buf[0];
};

struct arm_mbox_msg {
	uint32_t size;
	uint32_t code;
#define MBC_PROCESS 0x00000000
#define MBC_SUCCESS 0x80000000
#define MBC_ERROR   0x80000001
	uint8_t data[0];
};

uint8_t static_mbox_msg[32] __attribute__((aligned(16)));
uint32_t mbox_address = 0xFE00B880;
bool mbox_vaddr = false;
#define MBOX_READ   (mbox_address + 0x00)
#define MBOX_STATUS (mbox_address + 0x18)
#define MBOX_WRITE  (mbox_address + 0x20)

#define WRITE32TO(addr, val) *(volatile uint32_t *)(addr) = (val)
#define READ32FROM(addr)     (*(volatile uint32_t *)(addr))

extern bool mmu_on;
void write_mbox(struct arm_mbox_msg *msg, uint8_t chan)
{
	uint32_t val = (uint32_t)msg;
	if ((val & 0xF) != 0)
		printf("mbox: misaligned message 0x%x\n", msg);
	if (mbox_vaddr) {
		val = kmem_lookup_phys(msg);
	}
	val += chan;
	while (READ32FROM(MBOX_STATUS) & 0x80000000) {
		/* wait for mbox to not be full */
	};
	WRITE32TO(MBOX_WRITE, val);
}

struct arm_mbox_msg *read_mbox(uint8_t chan)
{
	uint32_t val;
	do {
		while (READ32FROM(MBOX_STATUS) & 0x40000000) {
			/* wait for mbox to not be empty */
		}
		val = READ32FROM(MBOX_READ);
	} while ((val & 0xF) != chan);
	return (struct arm_mbox_msg *)(val & 0xFFFFFFF0);
}

#define MBOX_CHAN_PTAG         8
#define PTAG_ID_SET_GPIO_STATE 0x00038041

void mbox_set_led_state(int pin, int state)
{
	struct arm_mbox_msg *msg = (struct arm_mbox_msg *)static_mbox_msg;
	struct arm_mbox_ptag *ptag = (struct arm_mbox_ptag *)msg->data;
	uint32_t *value = (uint32_t *)ptag->buf;
	if (pin < 0)
		pin = LED_ACT;
	ptag->tagid = PTAG_ID_SET_GPIO_STATE;
	ptag->size = 8;
	ptag->code = 0;
	value[0] = pin;
	value[1] = state;
	value[2] = 0;
	msg->code = MBC_PROCESS;
	msg->size = sizeof(struct arm_mbox_msg) + sizeof(struct arm_mbox_ptag) +
	            3 * 4;
	write_mbox(msg, MBOX_CHAN_PTAG);
	read_mbox(MBOX_CHAN_PTAG);
}

void mbox_remap(void)
{
	mbox_address = kmem_remap_periph(mbox_address);
	mbox_vaddr = true;
}
