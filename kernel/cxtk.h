/*
 * Context tracker
 */

#pragma once

#include <stdint.h>

//#define CXTK

#ifdef CXTK
void cxtk_init(void);
void cxtk_track_syscall(void);
void cxtk_track_syscall_return(void);
void cxtk_track_irq(uint8_t id, uint32_t instr);
void cxtk_track_proc(void);
void cxtk_track_schedule(void);
#else
#define cxtk_init()                                                            \
	do {                                                                   \
	} while (0);
#define cxtk_track_syscall()                                                   \
	do {                                                                   \
	} while (0);
#define cxtk_track_syscall_return()                                            \
	do {                                                                   \
	} while (0);
#define cxtk_track_irq(id, instr)                                              \
	do {                                                                   \
	} while (0);
#define cxtk_track_proc()                                                      \
	do {                                                                   \
	} while (0);
#define cxtk_track_schedule()                                                  \
	do {                                                                   \
	} while (0);
#endif
void cxtk_report(void);
