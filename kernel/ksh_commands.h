#include "ksh.h"

extern struct ksh_cmd blk_ksh_cmds[];

#define KSH_SUB_COMMANDS KSH_SUB("blk", blk_ksh_cmds, "block commands"),
