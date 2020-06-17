#pragma once
#include <stdint.h>

#include "blk.h"

struct __attribute__((packed)) fat_bpb {
	uint8_t BS_jmpBoot[3];
	char BS_OEMName[8];
	uint16_t BS_BytsPerSec;
	uint8_t BPB_SecPerClus;
	uint16_t BPB_RsvdSecCnt;
	uint8_t BPB_NumFATs;
	uint16_t BPB_RootEntCnt;
	uint16_t BPB_TotSec16;
	uint8_t BPB_Media;
	uint16_t BPB_FATSz16;
	uint16_t BPB_SecPerTrk;
	uint16_t BPB_NumHeads;
	uint32_t BPB_HiddSec;
	uint32_t BPB_TotSec32;
};

struct __attribute__((packed)) fat_ebpb1216 {
	uint8_t BS_DrvNum;
	uint8_t BS_Reserved1;
	uint8_t BS_BootSig;
	uint32_t BS_VolID;
	char BS_VolLab[11];
	char BS_FilSysType[8];
	uint8_t _pad0[448];
	uint16_t Signature_word;
};

struct __attribute__((packed)) fat_ebpb32 {
	uint32_t BPB_FATSz32;
	uint16_t BPB_ExtFlags;
	uint16_t BPB_FSVer;
	uint32_t BPB_RootClus;
	uint16_t BPB_FSInfo;
	uint16_t BPB_BkBootSec;
	uint8_t BPB_Reserved[12];
	uint8_t BS_DrvNum;
	uint8_t BS_Reserved1;
	uint8_t BS_BootSig;
	uint32_t BS_VolID;
	char BS_VolLab[11];
	char BS_FilSysType[8];
	uint8_t _pad0[420];
	uint16_t Signature_word;
};

struct __attribute__((packed)) fat1216_bpb {
	struct fat_bpb base;
	struct fat_ebpb1216 ext;
};

struct __attribute__((packed)) fat32_bpb {
	struct fat_bpb base;
	struct fat_ebpb32 ext;
};

struct fat_fs {
	struct blockdev *dev;
};

int cmd_fat(int argc, char **argv);
