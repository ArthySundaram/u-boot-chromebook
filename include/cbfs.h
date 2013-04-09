/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __CBFS_H
#define __CBFS_H

#include <linux/compiler.h>
#include <compiler.h>
#include <common.h>

typedef enum CbfsResult {
	CBFS_SUCCESS = 0,
	CBFS_NOT_INITIALIZED,
	CBFS_BAD_HEADER,
	CBFS_BAD_FILE,
	CBFS_FILE_NOT_FOUND
} CbfsResult;

typedef enum CbfsFileType {
	CBFS_TYPE_STAGE = 0x10,
	CBFS_TYPE_PAYLOAD = 0x20,
	CBFS_TYPE_OPTIONROM = 0x30,
	CBFS_TYPE_BOOTSPLASH = 0x40,
	CBFS_TYPE_RAW = 0x50,
	CBFS_TYPE_VSA = 0x51,
	CBFS_TYPE_MBI = 0x52,
	CBFS_TYPE_MICROCODE = 0x53,
	CBFS_COMPONENT_CMOS_DEFAULT = 0xaa,
	CBFS_COMPONENT_CMOS_LAYOUT = 0x01aa
} CbfsFileType;

typedef struct CbfsHeader {
	u32 magic;
	u32 version;
	u32 romSize;
	u32 bootBlockSize;
	u32 align;
	u32 offset;
	u32 pad[2];
} __packed CbfsHeader;

typedef struct CbfsFileHeader {
	u8 magic[8];
	u32 len;
	u32 type;
	u32 checksum;
	u32 offset;
} __packed CbfsFileHeader;

typedef struct CbfsCacheNode {
	struct CbfsCacheNode *next;
	u32 type;
	void *data;
	u32 dataLength;
	char *name;
	u32 nameLength;
	u32 checksum;
} __packed CbfsCacheNode;

#define PAYLOAD_SEGMENT_CODE   0x45444F43
#define PAYLOAD_SEGMENT_DATA   0x41544144
#define PAYLOAD_SEGMENT_BSS    0x20535342
#define PAYLOAD_SEGMENT_PARAMS 0x41524150
#define PAYLOAD_SEGMENT_ENTRY  0x52544E45

#define CBFS_COMPRESS_NONE  0
#define CBFS_COMPRESS_LZMA  1

typedef struct CbfsPayloadSegment {
	uint32_t type;
	uint32_t compression;
	uint32_t offset;
	uint64_t load_addr;
	uint32_t len;
	uint32_t mem_len;
} __packed CbfsPayloadSegment;

typedef const struct CbfsCacheNode *CbfsFile;

extern CbfsResult file_cbfs_result;

/*
 * Return a string describing the most recent error condition.
 *
 * @return A pointer to the constant string.
 */
const char *file_cbfs_error(void);

/*
 * Initialize the CBFS driver and load metadata into RAM.
 *
 * @param end_of_rom	Points to the end of the ROM the CBFS should be read
 *                      from.
 */
void file_cbfs_init(uintptr_t end_of_rom);

/*
 * Get the header structure for the current CBFS.
 *
 * @return A pointer to the constant structure, or NULL if there is none.
 */
const CbfsHeader *file_cbfs_get_header(void);

/*
 * Get a handle for the first file in CBFS.
 *
 * @return A handle for the first file in CBFS, NULL on error.
 */
CbfsFile file_cbfs_get_first(void);

/*
 * Get a handle to the file after this one in CBFS.
 *
 * @param file		A pointer to the handle to advance.
 */
void file_cbfs_get_next(CbfsFile *file);

/*
 * Find a file with a particular name in CBFS.
 *
 * @param name		The name to search for.
 *
 * @return A handle to the file, or NULL on error.
 */
CbfsFile file_cbfs_find(const char *name);


/***************************************************************************/
/* All of the functions below can be used without first initializing CBFS. */
/***************************************************************************/

/*
 * Find a file with a particular name in CBFS without using the heap.
 *
 * @param end_of_rom	Points to the end of the ROM the CBFS should be read
 *                      from.
 * @param name		The name to search for.
 *
 * @return A handle to the file, or NULL on error.
 */
CbfsFile file_cbfs_find_uncached(uintptr_t end_of_rom, const char *name);

/*
 * Get the name of a file in CBFS.
 *
 * @param file		The handle to the file.
 *
 * @return The name of the file, NULL on error.
 */
const char *file_cbfs_name(CbfsFile file);

/*
 * Get the size of a file in CBFS.
 *
 * @param file		The handle to the file.
 *
 * @return The size of the file, zero on error.
 */
u32 file_cbfs_size(CbfsFile file);

/*
 * Get the type of a file in CBFS.
 *
 * @param file		The handle to the file.
 *
 * @return The type of the file, zero on error.
 */
u32 file_cbfs_type(CbfsFile file);

/*
 * Read a file from CBFS into RAM
 *
 * @param file		A handle to the file to read.
 * @param buffer	Where to read it into memory.
 *
 * @return If positive or zero, the number of characters read. If negative, an
 *         error occurred.
 */
long file_cbfs_read(CbfsFile file, void *buffer, unsigned long maxsize);

/*
 * Print CBFS inventory
 *
 * @param cmdtp         Should be NULL
 * @param flag          Should be 0
 * @param argc          Should be 0
 * @param argv          Should be NULL
 *
 */
int do_cbfs_ls(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]);

#endif /* __CBFS_H */
