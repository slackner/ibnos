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

#include <memory/physmem.h>
#include <memory/paging.h>
#include <console/console.h>
#include <util/util.h>

/**
 * \defgroup PhysMem Physical memory
 * \addtogroup PhysMem
 * @{
 */

uint32_t ramSize = 0;
uint32_t ramUsableSize = 0;

#define PHYSMEMEXTRA_SIZE 0x1000 /* must match PAGE_SIZE for now */
#define PHYSMEMEXTRA_MASK 0x3FF
#define PHYSMEMEXTRA_BITS 10
#define PHYSMEMEXTRA_COUNT 0x400

struct physMemExtraInfo
{
	union
	{
		struct
		{
			uint32_t present		: 1;
			uint32_t ref			: 7;
			uint32_t unpageable		: 1;
			uint32_t avail			: 23;
		};
		uint32_t value;
	};
} __attribute__((packed));

static bool physMemInitialized = false;
static uint32_t physMemMap[(PAGE_COUNT + 31) / 32] __attribute__((aligned(4096)));
static struct physMemExtraInfo *physMemExtra[PHYSMEMEXTRA_COUNT] __attribute__((aligned(4096)));

/* used to determine the memory layout of the kernel itself */
extern uint32_t __kernelBegin;
extern uint32_t __kernelEnd;
#define LINKER_KERNEL_BEGIN ((uint32_t)&__kernelBegin)
#define LINKER_KERNEL_SIZE  ((uint32_t)&__kernelEnd - (uint32_t)&__kernelBegin)

static const char *error_outOfMemory[] =
{
	" OUT OF MEMORY ",
	"  The system ran out of physical memory!",
	NULL
};

uint32_t __getCR0();
asm(".text\n.align 4\n"
"__getCR0:\n"
"	movl %cr0, %eax\n"
"	ret\n"
);

/* Returns a pointer to the physMemExtraTable */
static struct physMemExtraInfo *__getPhysMemExtraInfo(uint32_t index, bool alloc)
{
	uint32_t i = index >> PHYSMEMEXTRA_BITS;
	assert(__getCR0() & 0x80000000);

	assert(i < PHYSMEMEXTRA_COUNT);
	if (!physMemExtra[i])
	{
		if (!alloc) return NULL;
		physMemExtra[i] = pagingAllocatePhysMem(NULL, 1, true, false);
		memset(physMemExtra[i], 0, PAGE_SIZE);

		/* TODO: mark it as unpageable? */
		physMemMarkUnpageable(pagingGetPhysMem(NULL, physMemExtra[i]));
	}

	return &((physMemExtra[i])[index & PHYSMEMEXTRA_MASK]);
}

/**
 * @brief Initializes the physical memory management
 * @details In order to implement physMemAllocPage() the operating system has to
 *			keep track of all used and unused physical pages. Internally this
 *			module works using a huge bitmap, where a 1 represents a used page, and
 *			0 an unused page. This function initializes all the structures using the
 *			memory layout information filled by the GRUB boot loader.
 *
 * @param bootInfo Bootinfo structure filled by the GRUB boot loader
 */
void physMemInit(multiboot_info_t* bootInfo)
{
	size_t offset = 0;

	assert(!physMemInitialized);
	assert(bootInfo);

	/* clear the extra info map */
	assert(((uint32_t)physMemExtra & PAGE_MASK) == 0);
	memset(physMemExtra, 0, sizeof(physMemExtra));

	/* determine ram size */
	assert(bootInfo->flags & MULTIBOOT_INFO_MEM_MAP); /* Is mmap_* valid? */
	ramSize = bootInfo->mem_upper;

	/* set the complete memory to reserved */
	physMemClearMemoryBits(PHYSMEM_RESERVED);

	/*
	 * Setup free memory regions
	 */
	assert(bootInfo->flags & MULTIBOOT_MEMORY_INFO); /* Is mem_{upper,lower} valid? */
	while (offset < bootInfo->mmap_length)
	{
		uint64_t startIndex, stopIndex;

		multiboot_memory_map_t *memMap = (multiboot_memory_map_t *)((char*)bootInfo->mmap_addr + offset);
		offset += sizeof(memMap->size) + memMap->size;

		if (!(memMap->type & MULTIBOOT_MEMORY_AVAILABLE))
			continue;

		/* process this entry */
		startIndex = (memMap->addr + PAGE_MASK) >> PAGE_BITS;
		stopIndex  = (memMap->addr + memMap->len) >> PAGE_BITS;

		if (stopIndex <= startIndex)
			continue;

		if (startIndex >= PAGE_COUNT)
			continue;

		if (stopIndex >= PAGE_COUNT)
			stopIndex = PAGE_COUNT - 1;

		/* set the available memory as free */
		physMemSetMemoryBits(startIndex, stopIndex - startIndex, PHYSMEM_FREE);
	}

	/*
	 * Protect the kernel itself
	 */
	physMemProtectBootEntry(LINKER_KERNEL_BEGIN, LINKER_KERNEL_SIZE);

	/*
	 * Protect useful boot information
	 */
	physMemProtectBootEntry((uint32_t)bootInfo, sizeof(*bootInfo));
	physMemProtectBootEntry(bootInfo->mmap_addr, bootInfo->mmap_length);

	/* Is the command line passed? */
	if (bootInfo->flags & MULTIBOOT_INFO_CMDLINE)
		physMemProtectBootEntry(bootInfo->cmdline, stringLength((char*)bootInfo->cmdline));

	/* Are mods_* valid? */
	if (bootInfo->flags & MULTIBOOT_INFO_MODS)
	{
		physMemProtectBootEntry(bootInfo->mods_addr, bootInfo->mods_count * sizeof(multiboot_module_t));

		multiboot_module_t *module = (multiboot_module_t *)bootInfo->mods_addr;
		for (uint32_t i = 0; i < bootInfo->mods_count; i++)
		{
			if (module[i].mod_start >= module[i].mod_end)
				continue;

			physMemProtectBootEntry(module[i].mod_start, module[i].mod_end - module[i].mod_start);
		}
	}

	/*
	if (bootInfo->drives_addr && bootInfo->drives_length)
		physMemProtectBootEntry(bootInfo->drives_addr, bootInfo->drives_length);

	if (bootInfo->config_table)
		physMemProtectBootEntry(bootInfo->config_table, PAGE_SIZE);

	if (bootInfo->boot_loader_name)
		physMemProtectBootEntry(bootInfo->boot_loader_name, stringLength((char*)bootInfo->boot_loader_name));

	if (bootInfo->apm_table)
		physMemProtectBootEntry(bootInfo->apm_table, sizeof(struct multiboot_apm_info));
	*/

	physMemInitialized = true;
}

/**
 * @brief Query RAM size
 * @details Returns the amount of available physical memory. This information is
 *			only available after the physical memory management was initialized
 *			by calling physMemInit().
 * @return Available RAM size in bytes
 */
uint32_t physMemRAMSize()
{
	return ramSize;
}

/**
 * @brief Query usable RAM size
 * @details Returns the amount of useable physical memory. This is similar to
 *			physMemRAMSize(), except that all unpageable and kernel related pages
 *			are subtracted. This information is only available after the physical
 *			memory management was initialized.
 * @return Useable RAM size in bytes
 */
uint32_t physMemUsableMemory()
{
	/* FIXME: Calculate appropriate usable ram size */
	NOTIMPLEMENTED();
	return ramUsableSize;
}

/**
 * @brief Marks the full memory range as reserved or free
 *
 * @param reserved If true the whole memory is marked as reserved, otherwise as free.
 */
void physMemClearMemoryBits(bool reserved)
{
	uint32_t mask = reserved ? 0xFFFFFFFF : 0;
	uint32_t longIndex;

	for (longIndex = 0; longIndex < sizeof(physMemMap) / sizeof(physMemMap[0]); longIndex++)
		physMemMap[longIndex] = mask;
}

/**
 * @brief Marks all pages within a memory range as reserved and adds them to the boot map
 *
 * @param addr Physical base address of the memory block
 * @param length Length of the memory block in bytes
 */
void physMemProtectBootEntry(uint32_t addr, uint32_t length)
{
	uint32_t startIndex = addr >> PAGE_BITS;
	uint32_t stopIndex  = ((uint64_t)addr + length + PAGE_MASK) >> PAGE_BITS;

	if (stopIndex >= PAGE_COUNT)
		stopIndex = PAGE_COUNT - 1;

	assert(startIndex <= stopIndex);

	pagingInsertBootMap(startIndex, stopIndex);
	physMemSetMemoryBits(startIndex, stopIndex - startIndex, PHYSMEM_RESERVED);
	return;
}

/**
 * @brief Marks all pages within a memory range as reserved
 *
 * @param addr Physical base address of the memory block
 * @param length Length of the memory block in bytes
 */
void physMemReserveMemory(uint32_t addr, uint32_t length)
{
	uint32_t startIndex = addr >> PAGE_BITS;
	uint32_t stopIndex  = ((uint64_t)addr + length + PAGE_MASK) >> PAGE_BITS;

	if (stopIndex >= PAGE_COUNT)
		stopIndex = PAGE_COUNT - 1;

	assert(startIndex <= stopIndex);

	physMemSetMemoryBits(startIndex, stopIndex - startIndex, PHYSMEM_RESERVED);
}

/**
 * @brief Marks all pages fully contained within the memory range as free
 *
 * @param addr Physical base address of the memory block
 * @param length Length of the memory block in bytes
 */
void physMemFreeMemory(uint32_t addr, uint32_t length)
{
	uint32_t startIndex = (addr + PAGE_MASK) >> PAGE_BITS;
	uint32_t stopIndex  = ((uint64_t)addr + length) >> PAGE_BITS;

	if (stopIndex >= PAGE_COUNT)
		stopIndex = PAGE_COUNT - 1;

	assert(startIndex <= stopIndex);

	physMemSetMemoryBits(startIndex, stopIndex - startIndex, PHYSMEM_FREE);
}

/**
 * @brief Marks a specific range of pages as reserved or free
 *
 * @param startIndex Physical index of the first page which should be modified
 * @param length Number of pages which should be changed
 * @param reserved If true the pages are marked as reserved, otherwise as free.
 */
void physMemSetMemoryBits(uint32_t startIndex, uint32_t length, bool reserved)
{
	uint32_t longIndex, longOffset;
	uint32_t mask;

	assert(length < PAGE_COUNT);
	assert(startIndex < PAGE_COUNT - length);

	/* get index and offset */
	longIndex  = startIndex >> 5;
	longOffset = startIndex & 31;

	/* handle all bits till the next dword boundary */
	if (longOffset)
	{

		/* number of bits reaches into the next dword */
		if (length > 32 - longOffset)
		{
			mask = 0xFFFFFFFF << longOffset;
			length -= (32 - longOffset);
		}
		else
		{
			mask = ((1 << length) - 1) << longOffset;
			length = 0;
		}

		if (reserved)
			physMemMap[longIndex++] |= mask;
		else
			physMemMap[longIndex++] &= ~mask;
	}

	/*  handle full dwords */
	mask = reserved ? 0xFFFFFFFF : 0;
	while (length >= 32)
	{
		physMemMap[longIndex++] = mask;
		length -= 32;
	}

	/* handle rest */
	if (length > 0)
	{
		mask = (1 << length) - 1;

		if (reserved)
			physMemMap[longIndex] |= mask;
		else
			physMemMap[longIndex] &= ~mask;
	}
}

/**
 * @brief Allocates a page of physical memory
 * @details This command searches for an unused page in the physical memory bitmap
 *			and afterwards marks the specific page as reserved. If there is no
 *			physical memory left then the algorithm tries to page out some memory
 *			to the hard drive. If this fails then a system failure is triggered.
 *
 * @param lowmem If true then the search also includes the physical memory area below 1MB
 * @return Index of the physical page which was allocated
 */
uint32_t physMemAllocPage(bool lowmem)
{
	uint32_t try, longIndex, longOffset;

	for (try = 0; try < 0x10; try++)
	{

		for (longIndex = lowmem ? 0 : 8 /* 1MB */; longIndex < sizeof(physMemMap) / sizeof(physMemMap[0]); longIndex++)
		{
			if (physMemMap[longIndex] != 0xFFFFFFFF)
				break;
		}

		if (longIndex < sizeof(physMemMap) / sizeof(physMemMap[0]))
		{
			longOffset = 0;

			while ((physMemMap[longIndex] >> longOffset) & 1)
				longOffset++;

			physMemMap[longIndex] |= (1 << longOffset);

			return longIndex << 5 | longOffset;
		}

		/* try to page out some other stuff */
		physMemPageOut(1);
	}

	SYSTEM_FAILURE(error_outOfMemory, lowmem);
	return 0; /* never reached */
}

/**
 * @brief Releases a page of physical memory
 * @details Deallocates a page of physical memory, or if the page has a refcount of
 *			greater than 1, then decrements the refcount. The refcounting menchanism
 *			ensures that the memory is not released before all processes containing
 *			the same physical memory page have released it again.
 *
 * @param index Index of the physical page to deallocate
 * @return New value of the refcounter (0 = the page was deallocated)
 */
uint32_t physMemReleasePage(uint32_t index)
{
	struct physMemExtraInfo *info;
	uint32_t longIndex, longOffset;

	longIndex  = index >> 5;
	longOffset = index & 31;

	/* You can not released a free page */
	assert(((physMemMap[longIndex] >> longOffset) & 1));

	info = __getPhysMemExtraInfo(index, false);
	if (info && info->value)
	{
		assert(info->present);

		/* decrease refcount, only free page if we reach zero */
		if (--info->ref) return info->ref;

		/* reset */
		info->value = 0;
	}

	/* mark the page as free */
	physMemMap[longIndex] &= ~(1 << longOffset);
	return 0;
}

/**
 * @brief Increment the refcounter of a physical page
 *
 * @param index Index of the physical page
 * @return Index of the physical page
 */
uint32_t physMemAddRefPage(uint32_t index)
{
	struct physMemExtraInfo *info = __getPhysMemExtraInfo(index, true);
	assert(info);

	if (!info->value)
	{
		info->present	= 1;
		info->ref		= 1;
	}

	info->ref++;
	assert(info->ref);

	return index;
}

/**
 * @brief Marks a physical page as unpageable
 * @details Marks a physical page as unpageable, to ensure that it is never paged
 *			out to the hard drive. This is necessary for some kernel related pages,
 *			like GDT or interrupt handler functions.
 *
 * @param index Index of the physical page
 * @return Index of the physical page
 */
uint32_t physMemMarkUnpageable(uint32_t index)
{
	struct physMemExtraInfo *info = __getPhysMemExtraInfo(index, true);
	assert(info);

	if (!info->value)
	{
		info->present	= 1;
		info->ref		= 1;
	}

	info->unpageable = 1;

	return index;
}

/**
 * @brief Checks if a physical page is only referenced exactly one time
 * @details Looks up the refcount of a specific page, and returns true if the
 *			refcount is exactly one.
 *
 * @param index Index of the physical page
 * @return True if the page has a refcount of exactly one, otherwise false
 */
bool physMemIsLastRef(uint32_t index)
{
	struct physMemExtraInfo *info = __getPhysMemExtraInfo(index, false);
	return (!info || !info->value || info->ref == 1);
}

/**
 * @brief Pages out some memory to the hard drive
 *
 * @param length Number of pages to page out
 */
void physMemPageOut(UNUSED uint32_t length)
{
	NOTIMPLEMENTED();
}

/**
 * @brief Pages in some data from the hard drive
 * @details Allocates a physical page (which possibily pages out other stuff, if
 *			the physical memory is exhausted). Afterwards loads the data from the
 *			hard drive (including permission bits and other stuff which was modified).
 *
 * @param hdd_index Index to a page on the hard drive
 * @return Index of the physical page
 */
uint32_t physMemPageIn(UNUSED uint32_t hdd_index)
{
	NOTIMPLEMENTED();
	return 0;
}

/**
 * @brief Dumps information about the physical memory usage
 */
void physMemDumpMemInfo()
{
	uint32_t startIndex	= 0;
	bool reserved = physMemMap[0] & 1;
	uint32_t mask = reserved ? 0xFFFFFFFF : 0;

	uint32_t index	= 0;
	uint32_t longIndex = 0, longOffset = 0; /* initialization not necessary, but gets rid of warnings */

	uint32_t usableMemory = 0;

	consoleWriteString("PHYSICAL MEMORY MAP:\n\n");

	for (;;)
	{
		if (index < PAGE_COUNT)
		{
			longIndex  = index >> 5;
			longOffset = index & 31;

			if (physMemMap[longIndex] == mask)
			{
				index += (32 - longOffset);
				continue;
			}
			else if (((physMemMap[longIndex] >> longOffset) & 1) == reserved)
			{
				index++;
				continue;
			}
		}

		consoleWriteHex32(startIndex << PAGE_BITS);
		consoleWriteString(" - ");
		consoleWriteHex32((index << PAGE_BITS) - 1);

		if (reserved)
			consoleWriteString(" RESERVED\n");
		else
		{
			consoleWriteString(" FREE\n");
			usableMemory += (index - startIndex) << PAGE_BITS;
		}

		if (index >= PAGE_COUNT)
			break;

		startIndex = index++;
		reserved = (physMemMap[longIndex] >> longOffset) & 1;
		mask     = reserved ? 0xFFFFFFFF : 0;
	}

	consoleWriteString("\nUsable Memory: ");
	consoleWriteHex32(usableMemory);
	consoleWriteString("\n\n");
}

/**
 * @}
 */
