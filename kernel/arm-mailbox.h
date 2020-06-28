#pragma once

void mbox_set_led_state(int pin, int state);
void mbox_remap(void);
#define LED_PWR       130
#define LED_ACT       42
#define led_pwr_on()  mbox_set_led_state(LED_PWR, 0)
#define led_pwr_off() mbox_set_led_state(LED_PWR, 1)
#define led_act_on()  mbox_set_led_state(LED_ACT, 1)
#define led_act_off() mbox_set_led_state(LED_ACT, 0)
