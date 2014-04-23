/*
 * Copyright (c) 2014, Michael Müller
 * Copyright (c) 2014, Sebastian Lackner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _H_PHYSMEM_
#define _H_PHYSMEM_

/** \addtogroup PhysMem
 *  @{
 */

#define PAGE_SIZE 0x1000
#define PAGE_MASK 0xFFF
#define PAGE_BITS 12
#define PAGE_COUNT 0x100000

#ifdef __KERNEL__

	#include <stdint.h>
	#include <stdbool.h>

	#include <util/util.h>
	#include <multiboot/multiboot.h>

	#define PHYSMEM_FREE     0
	#define PHYSMEM_RESERVED 1

	void physMemInit(multiboot_info_t* bootInfo);

	uint32_t physMemRAMSize();
	uint32_t physMemUsableMemory();

	void physMemProtectBootEntry(uint32_t addr, uint32_t length);

	void physMemClearMemoryBits(bool reserved);
	void physMemReserveMemory(uint32_t addr, uint32_t length);
	void physMemFreeMemory(uint32_t addr, uint32_t length);
	void physMemSetMemoryBits(uint32_t startIndex, uint32_t length, bool reserved);

	uint32_t physMemAllocPage(bool lowmem);
	uint32_t physMemReleasePage(uint32_t index);

	uint32_t physMemAddRefPage(uint32_t index);
	uint32_t physMemMarkUnpageable(uint32_t index);
	bool physMemIsLastRef(uint32_t index);

	void physMemPageOut(UNUSED uint32_t length);
	uint32_t physMemPageIn(UNUSED uint32_t hdd_index);

	void physMemDumpMemInfo();

#endif
/**
 *  @}
 */
#endif