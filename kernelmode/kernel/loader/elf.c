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

#include <loader/elf.h>
#include <process/process.h>
#include <memory/paging.h>
#include <memory/allocator.h>
#include <util/util.h>
#include <util/list.h>

struct requiredPages
{
	struct linkedList entry;
	uint32_t startIndex;
	uint32_t stopIndex;
};

void __insertRequiredPage(struct linkedList *pages, uint32_t startIndex, uint32_t stopIndex)
{
	struct requiredPages *it = LL_ENTRY(pages->next, struct requiredPages, entry);
	struct requiredPages *temp_it;

	if (stopIndex <= startIndex)
		return;

	while (&it->entry != pages)
	{
		if (stopIndex < it->startIndex)
			break;

		if (it->stopIndex < startIndex)
		{
			it = LL_ENTRY(it->entry.next, struct requiredPages, entry);
			continue;
		}

		if (startIndex >= it->startIndex && stopIndex <= it->stopIndex)
			return;

		if (it->startIndex < startIndex)
			startIndex = it->startIndex;

		if (it->stopIndex > stopIndex)
			stopIndex = it->stopIndex;

		/* remove entry */
		temp_it = LL_ENTRY(it->entry.next, struct requiredPages, entry);
		ll_remove(&it->entry);
		heapFree(it);
		it = temp_it;
	}

	/* insert entry */
	temp_it = heapAlloc(sizeof(*temp_it));
	assert(temp_it);

	temp_it->startIndex = startIndex;
	temp_it->stopIndex  = stopIndex;

	ll_add_before(&it->entry, &temp_it->entry);
}

/**
 * \defgroup ELF ELF Binary Loader
 * \addtogroup ELF
 *  @{
 *
 *	The ELF loader allows to load statically linked, non relocatable
 *	executables from the memory into an (empty) process.
 */

/**
 * @brief Loads an ELF executable stored in the memory into a process.
 *
 * @param p The process into which the executable should be loaded
 * @param addr The address where the ELF file is in memory
 * @param length The length of the file in memory
 * @return True, if successful or false otherwise
 */
bool elfLoadBinary(struct process *p, void *addr, uint32_t length)
{
	struct elfHeader *header = (struct elfHeader*) addr;
	struct userMemory k;

	/* check whether this can be a vlid ELF file */
	if (length < sizeof(*header))
		return false;

	/* check file flags */
	if (header->ident[0] != ELF_MAG0 || header->ident[1] != ELF_MAG1 ||
		header->ident[2] != ELF_MAG2 || header->ident[3] != ELF_MAG3)
		return false;

	struct elfSectionTable *section;
	struct linkedList pages = LL_INIT( pages );
	struct requiredPages *it, *__it;

	if (length < header->shoff)
		return false;

	section = (struct elfSectionTable *)((void*)header + header->shoff);
	for (uint32_t index = 0; index < header->shnum; index++)
	{
		section = (struct elfSectionTable *)((void*)section + header->shentsize);

		if ((uint32_t)section + sizeof(*section) - (uint32_t)addr > length)
			return false;

		if (!section->addr)
			continue;

		if (section->type != ELF_STYPE_NOBITS && length < ((uint64_t)section->offset + section->size))
			return false;

		uint32_t startIndex	= section->addr >> PAGE_BITS;
		uint32_t stopIndex	= ((uint64_t)section->addr + section->size + PAGE_MASK) >> PAGE_BITS;

		if (stopIndex >= PAGE_COUNT)
			stopIndex = PAGE_COUNT - 1;

		assert(startIndex <= stopIndex);

		__insertRequiredPage(&pages, startIndex, stopIndex);
	}

	LL_FOR_EACH_SAFE( it, __it, &pages, struct requiredPages, entry )
	{
		pagingAllocatePhysMemFixed(p, (void *)(it->startIndex << PAGE_BITS), it->stopIndex - it->startIndex, true, true);
		ll_remove(&it->entry);
		heapFree(it);
	}

	section = (struct elfSectionTable *)((void*)header + header->shoff);
	for (uint32_t index = 0; index < header->shnum; index++)
	{
		section = (struct elfSectionTable *)((void*)section + header->shentsize);

		if ((uint32_t)section + sizeof(*section) - (uint32_t)addr > length)
			return false;

		if (!section->addr)
			continue;

		if (!ACCESS_USER_MEMORY(&k, p, (void *)section->addr, section->size, true))
			return false;

		if (section->type == ELF_STYPE_NOBITS)
			memset(k.addr, 0, section->size);
		else
			memcpy(k.addr, addr + section->offset, section->size);

		RELEASE_USER_MEMORY(&k);
	}

	p->entryPoint = (void*)header->entry;
	return true;
}

/**
 * @}
 */