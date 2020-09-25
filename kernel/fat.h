#pragma once
#include <stdint.h>

#include "blk.h"
#include "fs.h"
#include "list.h"

struct __attribute__((packed)) fat_bpb {
	uint8_t BS_jmpBoot[3];
	char BS_OEMName[8];
	uint16_t BPB_BytsPerSec;
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

struct __attribute__((packed)) fat_dirent {
	char DIR_Name[11];
	uint8_t DIR_Attr;
	uint8_t DIR_NTRes;
	uint8_t DIR_CrtTimeTenth;
	uint16_t DIR_CrtTime;
	uint16_t DIR_CrtDate;
	uint16_t DIR_LstAccDate;
	uint16_t DIR_FstClusHI;
	uint16_t DIR_WrtTime;
	uint16_t DIR_WrtDate;
	uint16_t DIR_FstClusLO;
	uint32_t DIR_FileSize;
};

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20

#define FAT_ATTR_LONG_NAME                                                     \
	(FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM |              \
	 FAT_ATTR_VOLUME_ID)
#define FAT_ATTR_LONG_NAME_MASK                                                \
	(FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM |              \
	 FAT_ATTR_VOLUME_ID | FAT_ATTR_DIRECTORY | FAT_ATTR_ARCHIVE)

#define FAT12 0
#define FAT16 1
#define FAT32 2

struct fat_fs {
	struct fs fs;
	struct blkdev *dev;
	struct fat_bpb *bpb;
	uint8_t *fat;
	union {
		struct fat_ebpb1216 *bpb1216;
		struct fat_ebpb32 *bpb32;
		void *bpbvoid;
	};
	int type;
	uint32_t RootDirSectors;
	uint32_t DataSec;
	uint32_t CountofClusters;
	uint32_t FatSec1;
	uint32_t FatSec2;
	uint32_t RootSec;
};

struct fat_file_private {
	uint64_t first_cluster;
	uint64_t current_cluster;
};

#define fat_priv(file) ((struct fat_file_private *)file->priv)
