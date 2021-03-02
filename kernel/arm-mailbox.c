/*
 * Driver of the mbox communication system, primarily used for Raspberry Pi.
 */
#include "arm-mailbox.h"
#include "kernel.h"
#include "ksh.h"
#include "string.h"
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

static int cmd_led(int argc, char **argv, int led, int reverse)
{
	int state = 0;
	if (argc != 1) {
		puts("usage: led [led] [on|off]\n");
		return 1;
	}
	if (strcmp(argv[0], "on") == 0)
		state = 1;
	mbox_set_led_state(led, state ^ reverse);
	printf("state: %d\n", state ^ reverse);
	return state ^ reverse;
}

static int cmd_led_pwr(int argc, char **argv)
{
	return cmd_led(argc, argv, LED_PWR, 1);
}

static int cmd_led_act(int argc, char **argv)
{
	return cmd_led(argc, argv, LED_ACT, 0);
}

struct ksh_cmd led_ksh_cmds[] = {
	KSH_CMD("pwr", cmd_led_pwr, "control the PWR LED"),
	KSH_CMD("act", cmd_led_act, "control the ACT LED"),
	{ 0 },
};
