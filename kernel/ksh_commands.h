#include "ksh.h"

extern struct ksh_cmd blk_ksh_cmds[];
extern struct ksh_cmd timer_ksh_cmds[];
extern struct ksh_cmd dtb_ksh_cmds[];
extern struct ksh_cmd proc_ksh_cmds[];
extern struct ksh_cmd fat_ksh_cmds[];
extern struct ksh_cmd sync_ksh_cmds[];

#define KSH_SUB_COMMANDS                                                       \
	KSH_SUB("blk", blk_ksh_cmds, "block commands"),                        \
	        KSH_SUB("timer", timer_ksh_cmds, "timer commands"),            \
	        KSH_SUB("dtb", dtb_ksh_cmds, "device tree commands"),          \
	        KSH_SUB("proc", proc_ksh_cmds, "process commands"),            \
	        KSH_SUB("fat", fat_ksh_cmds, "FAT commands"),                  \
	        KSH_SUB("sync", sync_ksh_cmds, "synchronization commands"),
