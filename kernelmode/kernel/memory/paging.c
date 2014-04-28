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

#include <memory/paging.h>
#include <memory/physmem.h>
#include <interrupt/interrupt.h>
#include <process/process.h>
#include <process/thread.h>
#include <console/console.h>
#include <util/list.h>
#include <util/util.h>

struct bootMapEntry
{
	uint32_t startIndex;
	uint32_t length;
};

#define MAX_BOOT_ENTRIES 1024
static struct bootMapEntry pagingBootMap[MAX_BOOT_ENTRIES];
static uint32_t pagingNumBootMaps = 0;

#define KERNEL_DIR_ENTRY (PAGETABLE_COUNT - 1)
#define KERNEL_DIR_ADDR  (((KERNEL_DIR_ENTRY << PAGETABLE_BITS) | KERNEL_DIR_ENTRY) << PAGE_BITS)
#define KERNEL_PAGE_ADDR (KERNEL_DIR_ENTRY << (PAGETABLE_BITS + PAGE_BITS))

static bool pagingInitialized = false;

static const char *error_virtualAddressInUse[] =
{
	" INTERNAL ERROR ",
	"  Requested virtual memory address is already in use",
	NULL
};

static const char *error_virtualAddressSpaceFull[] =
{
	" OUT OF MEMORY ",
	"  Unable to fulfill request because the virtual address space is exhausted",
	NULL
};

/* possible flags when present == 0 */
#define PAGING_AVAIL_NOTPRESENT_RESERVED				1 /* frame == 0, denies allocation for everyone with lower privileges */
#define PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE		2 /* frame == 0, creates a new page on access with rw = 1 */
#define PAGING_AVAIL_NOTPRESENT_OUTPAGED				3 /* frame points to some external device where the page is located */

/* possible flags when present == 1 */
#define PAGING_AVAIL_PRESENT_SHARED						1 /* shared, will not be duplicated when forking */
#define PAGING_AVAIL_PRESENT_NO_FORK					2 /* doesn't copy this block while forking */
#define PAGING_AVAIL_PRESENT_ON_WRITE_DUPLICATE			3 /* readwrite=0, copy (and release) the physical frame pointed to by frame */

uint32_t __getCR0();
asm(".text\n.align 4\n"
"__getCR0:\n"
"	movl %cr0, %eax\n"
"	ret\n"
);

uint32_t __setCR0(uint32_t value);
asm(".text\n.align 4\n"
"__setCR0:\n"
"	movl 4(%esp), %eax\n"
"	movl %eax, %cr0\n"
"	ret\n"
);

uint32_t __getCR3();
asm(".text\n.align 4\n"
"__getCR3:\n"
"	movl %cr3, %eax\n"
"	ret\n"
);

uint32_t __setCR3(uint32_t value);
asm(".text\n.align 4\n"
"__setCR3:\n"
"	movl 4(%esp), %eax\n"
"	movl %eax, %cr3\n"
"	ret\n"
);

static inline void __flushTLBSingle(void *addr)
{
   asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}

static inline bool __isReserved(struct pagingEntry *table)
{
	return !table->present && (table->avail == PAGING_AVAIL_NOTPRESENT_RESERVED);
}


static void *__pagingMapPhysMem(struct process *p, uint32_t index, void *addr, bool rw, bool user);

/* Returns a pointer to the pagingEntry element for a specific virtual address.
 * Can be NULL if there is no page table for the specific address yet and alloc is set to false */
static struct pagingEntry *__getPagingEntry(struct process *p, void *addr, bool alloc)
{
	struct pagingEntry *dir, *table;
	bool pagingEnabled	= (__getCR0() & 0x80000000);
	uint32_t i;

	if (pagingEnabled)
	{
		/* lookup the paging directory corresponding to the process */
		dir = (p != NULL) ? p->pageDirectory : (struct pagingEntry *)KERNEL_DIR_ADDR;
	}
	else
	{
		/* paging not enabled, request for kernel paging directory */
		assert(p == NULL);
		dir = (struct pagingEntry *)__getCR3();
	}

	i = (uint32_t)addr >> (PAGETABLE_BITS + PAGE_BITS);
	dir += i;

	if (!dir->value)
	{
		if (!alloc) return NULL;

		/* allocate a new entry */
		dir->present	= 1;
		dir->rw			= 1;
		dir->user		= 1;
		dir->frame		= physMemAllocPage(false);
	}
	else alloc = false;

	if (!dir->present)
	{
		switch (dir->avail)
		{
			case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
				physMemPageIn(dir->frame);
				break;

			case PAGING_AVAIL_NOTPRESENT_RESERVED:
			case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
			default:
				assert(0);
		}

		assert(dir->present);
	}

	/* special flags on dir entries not allowed yet */
	assert(!dir->avail);

	if (pagingEnabled)
	{
		if (p != NULL)
		{
			/* map it into the kernel if its not present yet */
			if (!p->pageTables[i])
				p->pageTables[i] = __pagingMapPhysMem(NULL, physMemAddRefPage(dir->frame), NULL, true, false);

			/* then calculate the position of the entry in the page table */
			table = p->pageTables[i] + (((uint32_t)addr >> PAGE_BITS) & PAGETABLE_MASK);
		}
		else
		{
			/* kernel mode pages are always mapped into the kernel virtual address space */
			table = ((struct pagingEntry *)KERNEL_PAGE_ADDR + ((uint32_t)addr >> PAGE_BITS));
		}
	}
	else
	{
		/* paging not enabled, request for kernel pages */
		assert(p == NULL);
		table = ((struct pagingEntry *)(dir->frame << PAGE_BITS) + (((uint32_t)addr >> PAGE_BITS) & PAGETABLE_MASK));
	}

	/* clear the whole page if this is freshly allocated memory */
	if (alloc)
		memset((void *)((uint32_t)table & ~PAGE_MASK), 0, PAGE_SIZE);

	return table;
}

/* helper for pagingInit */
static bool __pagingBootMapCheck(uint32_t startIndex, uint32_t stopIndex)
{
	uint32_t i;

	/* TODO: binary search would be faster ... */
	for (i = 0; i < pagingNumBootMaps; i++)
	{
		if (stopIndex <= pagingBootMap[i].startIndex)
			break;

		if (pagingBootMap[i].startIndex + pagingBootMap[i].length <= startIndex)
			continue;

		/* overlapping */
		return true;
	}

	return false;
}

/* Maps some physical memory to a given addr (or to any) */
static void *__pagingMapPhysMem(struct process *p, uint32_t index, void *addr, bool rw, bool user)
{
	struct pagingEntry *table;

	if (addr)
	{
		/* we don't allow mapping something in the NULL page for now */
		assert(((uint32_t)addr & ~PAGE_MASK) != 0);

		table = __getPagingEntry(p, addr, true);
		if (table->value)
		{
			SYSTEM_FAILURE(error_virtualAddressInUse, (uint32_t)addr);
			return NULL;
		}
	}
	else
	{
		uint32_t i;

		/* TODO: Make this more efficient */
		for (i = 1; i < KERNEL_DIR_ENTRY * PAGETABLE_COUNT; i++)
		{
			table = __getPagingEntry(p, (void *)(i << PAGE_BITS), true);
			if (!table->value)
			{
				addr = (void *)(i << PAGE_BITS);
				break;
			}
		}

		if (!addr)
		{
			SYSTEM_FAILURE(error_virtualAddressSpaceFull);
			return NULL;
		}

	}

	/* reset */
	table->value	= 0;

	table->present	= 1;
	table->rw		= rw;
	table->user		= user;
	table->frame	= index;

	if (p == NULL) __flushTLBSingle(addr);
	return addr;
}

/**
 * @brief Page fault handler
 * @details Whenever the kernel or a usermode application tries to access a
 *			virtual memory address which is not present in the page table (or where
 *			access is denied) a page fault is triggered. If the situation can be
 *			resolved (for example loading the missing page from the harddrive or
 *			duplicating the read-only page, then #INTERRUPT_CONTINUE_EXECUTION is
 *			returned. Otherwise the handler cannot resolve the problem. This is
 *			critical for kernel errors because the kernel is unable to proceed.
 *			For usermode applications the corresponding process will be killed - at
 *			some later point we will probably add some additional mechanisms such
 *			that usermode applications can also try to resolve this error on their
 *			own by defining an exception handler function at program start.
 *
 * @param interrupt Always 0x0E
 * @param error Error code containing information about the type of access
 * @param t The thread in which the page fault occured
 * @return #INTERRUPT_CONTINUE_EXECUTION on success, otherwise #INTERRUPT_UNHANDLED
 */
uint32_t interrupt_0x0E(UNUSED uint32_t interrupt, uint32_t error, struct thread *t)
{
	struct process *p = t ? t->process : NULL;
	struct pagingEntry *table;
	void *cr2;
	bool user, write;
	UNUSED bool present;

	/* read cr2 */
	asm volatile("mov %%cr2, %0" : "=r" (cr2));

	user	= (error & 4) != 0;
	write	= (error & 2) != 0;
	present = (error & 1) != 0;

	/*
	consoleWriteString("page fault in ");
	consoleWriteHex32((uint32_t)cr2);
	consoleWriteString(", error = ");
	consoleWriteInt32(error);
	consoleWriteString("\n");
	*/

	/* ensure that error code (user / supervisor) matches where we have discovered the error */
	assert((p != NULL) == user);

	table = __getPagingEntry(p, cr2, false);
	if (!table || !table->value) return INTERRUPT_UNHANDLED;

	/* user has no access to this kernel page */
	if (!table->user && user) return INTERRUPT_UNHANDLED;

	if (!table->present)
	{
		switch (table->avail)
		{
			case PAGING_AVAIL_NOTPRESENT_RESERVED:
				return INTERRUPT_UNHANDLED;

			case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
				physMemPageIn(table->frame);
				break;

			case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
			default:
				assert(0);
		}

		assert(table->present);
	}

	if (!table->rw && write)
	{
		if (table->avail != PAGING_AVAIL_PRESENT_ON_WRITE_DUPLICATE)
			return INTERRUPT_UNHANDLED;

		/* duplicate this page */
		table->rw		= 1;
		table->avail	= 0;

		if (!physMemIsLastRef(table->frame))
		{
			uint32_t old_index = table->frame;
			table->frame = physMemAllocPage(false);
			void *destination	= __pagingMapPhysMem(NULL, physMemAddRefPage(table->frame), NULL, true, false);
			void *source		= __pagingMapPhysMem(NULL, physMemAddRefPage(old_index), NULL, true, false);

			memcpy(destination, source, PAGE_SIZE);

			pagingReleasePhysMem(NULL, destination, 1);
			pagingReleasePhysMem(NULL, source, 1);
			physMemReleasePage(old_index);
		}
	}

	if (p == NULL) __flushTLBSingle(cr2);

	return INTERRUPT_CONTINUE_EXECUTION;
}

/**
 * @brief Appends a specific range of physical pages to the bootmap
 * @details The bootmap is an internal array of physical page regions, which is
 *			mapped at exactly the same location in virtual address space after
 *			paging has been enabled. This allows to continue the execution of
 *			the kernel without having to relocate it to a different address.
 *			It is only possible to add entries until paging was activated,
 *			afterwards all these steps (requesting physical memory and mapping it
 *			to the corresponding virtual address) have to be done manually.
 *
 * @param startIndex Start index of the memory region
 * @param stopIndex Stop index of the memory region (not included in mapping)
 */
void pagingInsertBootMap(uint32_t startIndex, uint32_t stopIndex)
{
	int mapIndex, i;

	/* adding entries is only allowed when paging is disabled (for now) */
	assert(!pagingInitialized);

	/* Check whether we can expand an existing entry */
	for (mapIndex = 0; mapIndex < (signed)pagingNumBootMaps;)
	{
		/* we have to insert before the entry mapIndex */
		if (stopIndex < pagingBootMap[mapIndex].startIndex)
			break;

		/* not yet at the right position to insert elements */
		if (pagingBootMap[mapIndex].startIndex + pagingBootMap[mapIndex].length < startIndex)
		{
			mapIndex++;
			continue;
		}

		/* new region is fully included in an existing entry, nothing to do! */
		if (startIndex >= pagingBootMap[mapIndex].startIndex && stopIndex <= pagingBootMap[mapIndex].startIndex + pagingBootMap[mapIndex].length)
			return;

		/* overlapping area / possible to combine */
		if (pagingBootMap[mapIndex].startIndex < startIndex)
			startIndex = pagingBootMap[mapIndex].startIndex;

		if (pagingBootMap[mapIndex].startIndex + pagingBootMap[mapIndex].length > stopIndex)
			stopIndex = pagingBootMap[mapIndex].startIndex + pagingBootMap[mapIndex].length;

		/* remove entry */
		for (i = mapIndex + 1; i < (signed)pagingNumBootMaps; i++)
			pagingBootMap[i - 1] = pagingBootMap[i];
		pagingNumBootMaps--;
	}

	/* create a new entry */
	assert(pagingNumBootMaps < MAX_BOOT_ENTRIES);

	/* insert entry */
	for (i = pagingNumBootMaps - 1; i >= mapIndex; i--)
		pagingBootMap[i + 1] = pagingBootMap[i];
	pagingNumBootMaps++;

	pagingBootMap[mapIndex].startIndex = startIndex;
	pagingBootMap[mapIndex].length = stopIndex - startIndex;
}

/**
 * @brief Dumps a list of all entries in the boot map
 */
void pagingDumpBootMap()
{
	uint32_t i;

	consoleWriteString("PROTECTED BOOT ENTRIES:\n\n");

	for (i = 0; i < pagingNumBootMaps; i++)
	{
		consoleWriteHex32(pagingBootMap[i].startIndex << PAGE_BITS);
		consoleWriteString(" - ");
		consoleWriteHex32(((pagingBootMap[i].startIndex + pagingBootMap[i].length) << PAGE_BITS) - 1);
		consoleWriteString("\n");
	}
}

/**
 * @brief Initializes paging
 * @details This function is called as part of the startup routine to initialize
 *			all the processor features. It internally allocates a page directory
 *			for the kernel and afterwards prepares everything with the entries
 *			from the boot map. Then paging is finally enabled. It is currently
 *			not possible to turn it off again.
 */
void pagingInit()
{
	uint32_t pageDirectoryIndex, i;
	struct pagingEntry *dir;
	uint32_t index;

	assert(!pagingInitialized);

	/* the low and high memory addresses are reserved */
	assert(!__pagingBootMapCheck(0, 1));
	assert(!__pagingBootMapCheck(KERNEL_PAGE_ADDR >> PAGE_BITS, PAGE_COUNT - 1));

	pageDirectoryIndex = physMemAllocPage(false);

	/* initial setup of the page directory */
	dir	= (struct pagingEntry *)(pageDirectoryIndex << PAGE_BITS);
	memset(dir, 0, PAGE_SIZE);
	dir[KERNEL_DIR_ENTRY].present	= 1;
	dir[KERNEL_DIR_ENTRY].rw		= 1;
	dir[KERNEL_DIR_ENTRY].frame		= pageDirectoryIndex;
	__setCR3((uint32_t)dir);

	/* reserve all the remaining memory regions */
	for (i = 0; i < pagingNumBootMaps; i++)
	{
		for (index = pagingBootMap[i].startIndex; index < pagingBootMap[i].startIndex + pagingBootMap[i].length; index++)
			__pagingMapPhysMem(NULL, index, (void *)(index << PAGE_BITS), true, false);
	}

	/* enable paging */
	__setCR0(__getCR0() | 0x80000000);

	/* now mark all the kernel space as unpageable (requires paging to be initialized) */
	for (i = 0; i < pagingNumBootMaps; i++)
	{
		for (index = pagingBootMap[i].startIndex; index < pagingBootMap[i].startIndex + pagingBootMap[i].length; index++)
			physMemMarkUnpageable(index);
	}

	pagingInitialized = true;
}

/**
 * @brief Dumps information about the page table of a specific process
 *
 * @param p Pointer to a process object or NULL for the kernel
 */
void pagingDumpPageTable(struct process *p)
{
	struct pagingEntry *table;
	uint32_t i;

	consoleWriteString("PAGE TABLE MAP:\n\n");

	for (i = 0; i < PAGETABLE_COUNT * PAGETABLE_COUNT; i++)
	{
		table = __getPagingEntry(p, (void *)(i << PAGE_BITS), false);
		if (!table)
		{
			i |= PAGETABLE_MASK;
		}
		else if (table->value)
		{
			/* FIXME: support paging out stuff? */
			assert(table->present);

			consoleWriteHex32(i << PAGE_BITS);
			consoleWriteString(" -> ");
			consoleWriteHex32(table->frame << PAGE_BITS);
			consoleWriteString(", ");
		}
	}

}

/**
 * @brief Marks the memory in a specific memory area as reserved
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param addr Address where the memory should be mapped
 * @param length Number of consecutive pages which have to be unused
 * @param user If true then the user (ring3) also has access to the page, otherwise only the kernel
 */
void pagingReserveArea(struct process *p, void *addr, uint32_t length, bool user)
{
	struct pagingEntry *table;
	uint8_t *cur;

	/* TODO: Make this more efficient */
	for (cur = addr; length; length--, cur += PAGE_SIZE)
	{
		table = __getPagingEntry(p, cur, true);
		assert(!table->value);

		/* reset */
		table->value	= 0;

		table->present	= 0;
		table->rw		= 0;
		table->user		= user;
		table->avail	= PAGING_AVAIL_NOTPRESENT_RESERVED;
		table->frame	= 0;

		/* we don't clear the TLB since the pointer is still not valid */
	}
}

/**
 * @brief Searches for a consecutive area of length free pages in a process
 * @details Before mapping some memory to a process or into the kernel virtual
 *			address space we have to find a proper location. This function searches
 *			through the corresponding page table and afterwards returns a base
 *			address which can be used for later mapping. If the virtual address
 *			space is exhausted an exception is thrown, so only use this command if
 *			its very critical. For non-critical purposes use pagingTrySearchArea().
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param length Number of consecutive pages which have to be unused
 *
 * @return Virtual base address to the memory block (inside of the process)
 */
void *pagingSearchArea(struct process *p, uint32_t length)
{
	void *addr = pagingTrySearchArea(p, length);

	if (!addr)
	{
		SYSTEM_FAILURE(error_virtualAddressSpaceFull, length);
		return NULL;
	}

	return addr;
}

/**
 * @brief Searches for a consecutive area of length free pages in a process
 * @details Similar to pagingSearchArea(), but doesn't fail if the algorithm cannot
 *			find any spot in the corresponding page table. In such a case NULL will
 *			be returned.
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param length Number of consecutive pages which have to be unused
 *
 * @return Virtual base address to the memory block (inside of the process) or NULL
 */
void *pagingTrySearchArea(struct process *p, uint32_t length)
{
	struct pagingEntry *table;
	uint32_t i, start;
	void *addr = NULL;

	/* catch invalid arguments */
	if (!length)
		return NULL;

	/* TODO: Make this more efficient */
	for (i = 1, start = i; i < KERNEL_DIR_ENTRY * PAGETABLE_COUNT; i++)
	{
		table = __getPagingEntry(p, (void *)(i << PAGE_BITS), false);

		if (table && table->value)
			start = i + 1;
		else
		{
			if (!table) i |= PAGETABLE_MASK;
			if (i >= start + length - 1)
			{
				addr = (void *)(start << PAGE_BITS);
				break;
			}
		}
	}

	return addr;
}

/**
 * @brief Allocates several pages of physical memory in a process
 * @details This function first searches for a free range of consecutive virtual
 *			memory pages. Afterwards it maps these entries to physical memory and
 *			sets the access permissions based on the provided arguments. If the
 *			virtual address space is exhausted an exception is thrown, so only use
 *			this command if its very critical. For non-critical purposes use
 *			pagingTryAllocatePhysMem().
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param length Number of consecutive pages which have to be unused
 * @param rw If true then the page has write permission, otherwise it is a read-only page
 * @param user If true then the user (ring3) also has access to the page, otherwise only the kernel
 *
 * @return Virtual base address to the allocated memory block (inside of the process)
 */
void *pagingAllocatePhysMem(struct process *p, uint32_t length, bool rw, bool user)
{
	void *addr = pagingTryAllocatePhysMem(p, length, rw, user);

	if (!addr)
	{
		SYSTEM_FAILURE(error_virtualAddressSpaceFull, length);
		return NULL;
	}

	return addr;
}

/**
 * @brief Allocates several pages of unpageable physical memory in a process
 * @details Similar to pagingAllocatePhysMem(), but makes the physical memory
 *			unpageable before returning from the function. This is especially
 *			useful for memory which has to stay valid during the execution of
 *			exception handlers.
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param length Number of consecutive pages which have to be unused
 * @param rw If true then the page has write permission, otherwise it is a read-only page
 * @param user If true then the user (ring3) also has access to the page, otherwise only the kernel
 *
 * @return Virtual base address to the allocated memory block (inside of the process)
 */
void *pagingAllocatePhysMemUnpageable(struct process *p, uint32_t length, bool rw, bool user)
{
	struct pagingEntry *table;
	void *addr = pagingTrySearchArea(p, length);
	uint8_t *cur;
	uint32_t index;

	if (!addr)
	{
		SYSTEM_FAILURE(error_virtualAddressSpaceFull, length);
		return NULL;
	}

	/* reserve the whole area - this is necessary since we want to mark the
	 * memory as unpageable, which will probably again call a memory allocator */
	pagingReserveArea(p, addr, length, user);

	for (cur = addr; length; length--, cur += PAGE_SIZE)
	{
		index = physMemMarkUnpageable(physMemAllocPage(false));
		table = __getPagingEntry(p, cur, true);
		assert(__isReserved(table));

		/* reset */
		table->value	= 0;

		table->present	= 1;
		table->rw		= rw;
		table->user		= user;
		table->frame	= index;

		if (p == NULL) __flushTLBSingle(cur);
	}

	return addr;
}

/**
 * @brief Tries to allocates several pages of physical memory in a process
 * @details Similar to pagingAllocatePhysMem(), but doesn't fail if the algorithm
 *			cannot find any spot in the corresponding page table. In such a case
 *			NULL will be returned.
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param length Number of consecutive pages which have to be unused
 * @param rw If true then the page has write permission, otherwise it is a read-only page
 * @param user If true then the user (ring3) also has access to the page, otherwise only the kernel
 *
 * @return Virtual base address to the allocated memory block (inside of the process) or NULL
 */
void *pagingTryAllocatePhysMem(struct process *p, uint32_t length, bool rw, bool user)
{
	struct pagingEntry *table;
	void *addr = pagingTrySearchArea(p, length);
	uint8_t *cur;
	uint32_t index;

	if (!addr) return NULL;

	for (cur = addr; length; length--, cur += PAGE_SIZE)
	{
		index = physMemAllocPage(false);
		table = __getPagingEntry(p, cur, true);
		assert(!table->value);

		/* reset */
		table->value	= 0;

		table->present	= 1;
		table->rw		= rw;
		table->user		= user;
		table->frame	= index;

		if (p == NULL) __flushTLBSingle(cur);
	}

	return addr;
}

/**
 * @brief Allocates several pages of physical memory at a fixed virtual address in a process
 * @details Similar to pagingAllocatePhysMem(), but uses the provided address
 *			instead of searching through the virtual address space.
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param addr Address where the memory should be mapped
 * @param length Number of consecutive pages which have to be unused
 * @param rw If true then the page has write permission, otherwise it is a read-only page
 * @param user If true then the user (ring3) also has access to the page, otherwise only the kernel
 *
 * @return Virtual base address to the allocated memory block (inside of the process)
 */
void *pagingAllocatePhysMemFixed(struct process *p, void *addr, uint32_t length, bool rw, bool user)
{
	addr = pagingTryAllocatePhysMemFixed(p, addr, length, rw, user);

	if (!addr)
	{
		SYSTEM_FAILURE(error_virtualAddressInUse, (uint32_t)addr);
		return NULL;
	}

	return addr;
}

/**
 * @brief Allocates several pages of unpageable physical memory at a fixed virtual address in a process
 * @details Similar to pagingAllocatePhysMemFixed(), but makes the pages unpageable
 *			before returning from the function.
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param addr Address where the memory should be mapped
 * @param length Number of consecutive pages which have to be unused
 * @param rw If true then the page has write permission, otherwise it is a read-only page
 * @param user If true then the user (ring3) also has access to the page, otherwise only the kernel
 *
 * @return Virtual base address to the allocated memory block (inside of the process)
 */
void *pagingAllocatePhysMemFixedUnpageable(struct process *p, void *addr, uint32_t length, bool rw, bool user)
{
	struct pagingEntry *table;
	uint8_t *cur;
	uint32_t index;

	/* we don't allow mapping something in the NULL page for now */
	assert(((uint32_t)addr & ~PAGE_MASK) != 0);

	/* reserve the whole area - this is necessary since we want to mark the
	 * memory as unpageable, which will probably again call a memory allocator */
	pagingReserveArea(p, addr, length, user);

	for (cur = addr; length; length--, cur += PAGE_SIZE)
	{
		index = physMemMarkUnpageable(physMemAllocPage(false));
		table = __getPagingEntry(p, cur, true);
		assert(__isReserved(table));

		/* reset */
		table->value	= 0;

		table->present	= 1;
		table->rw		= rw;
		table->user		= user;
		table->frame	= index;

		if (p == NULL) __flushTLBSingle(cur);
	}

	return addr;
}

/**
 * @brief Allocates several pages of unpageable physical memory at a fixed virtual address in a process
 * @details Similar to pagingAllocatePhysMemFixed(), but doesn't fail if the algorithm
 *			cannot find any spot in the corresponding page table. In such a case
 *			NULL will be returned.
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param addr Address where the memory should be mapped
 * @param length Number of consecutive pages which have to be unused
 * @param rw If true then the page has write permission, otherwise it is a read-only page
 * @param user If true then the user (ring3) also has access to the page, otherwise only the kernel
 *
 * @return Virtual base address to the allocated memory block (inside of the process) or NULL
 */
void *pagingTryAllocatePhysMemFixed(struct process *p, void *addr, uint32_t length, bool rw, bool user)
{
	struct pagingEntry *table;
	uint8_t *cur;
	uint32_t index;

	/* we don't allow mapping something in the NULL page for now */
	if (((uint32_t)addr & ~PAGE_MASK) == 0) return NULL;

	for (cur = addr; length; length--, cur += PAGE_SIZE)
	{
		index = physMemAllocPage(false);
		table = __getPagingEntry(p, cur, true);
		if (table->value)
		{
			physMemReleasePage(index);
			pagingReleasePhysMem(NULL, addr, ((uint32_t)cur - (uint32_t)addr) >> PAGE_BITS);
			return NULL;
		}

		/* reset */
		table->value	= 0;

		table->present	= 1;
		table->rw		= rw;
		table->user		= user;
		table->frame	= index;

		if (p == NULL) __flushTLBSingle(cur);
	}

	return addr;
}


/* internally used for pagingReAllocatePhysMem */
static void *__pagingMove(struct process *p, void *dst_addr, void *src_addr, uint32_t length)
{
	struct pagingEntry *src, *dst;
	uint8_t *src_cur = src_addr, *dst_cur = dst_addr;

	/* both regions shouldn't be overlapping */
	assert( dst_cur + length <= src_cur || src_cur + length <= dst_cur );

	for (src_cur = src_addr, dst_cur = dst_addr; length; length--, src_cur += PAGE_SIZE, dst_cur += PAGE_SIZE)
	{
		src = __getPagingEntry(p, src_cur, false);
		assert(src && src->value);

		dst = __getPagingEntry(p, dst_cur, true);
		assert(!dst->value);

		/* copy the whole entry to the destination */
		*dst = *src;

		/* reset */
		src->value = 0;

		if (p == NULL)
		{
			__flushTLBSingle(src_cur);
			__flushTLBSingle(dst_cur);
		}
	}

	return (void *)((uint32_t)dst_addr | ((uint32_t)src_addr & PAGE_MASK));
}

/**
 * @brief Reallocates a specific range of virtual memory in a process
 * @details Changes the size of a block of virtual memory. If the new size is
 *			smaller than the old size all unused pages will be released. If the
 *			new size is greather than the old size some additional pages will be
 *			allocated with the provided permission attributes. If the reallocation
 *			fails a system failure will be triggered. This functions returns a
 *			pointer to the new location in the virtual address space of the given
 *			process.
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param addr Address pointing to some allocated blocks of memory
 * @param old_length Old length of the memory block in pages
 * @param new_length Number of requested pages
 * @param rw If true then the page has write permission, otherwise it is a read-only page
 * @param user If true then the user (ring3) also has access to the page, otherwise only the kernel
 * @return Virtual base address to the reallocated memory block (inside of the process)
 */
void *pagingReAllocatePhysMem(struct process *p, void *addr, uint32_t old_length, uint32_t new_length, bool rw, bool user)
{
	struct pagingEntry *table;
	uint8_t *cur;
	void *new_addr;
	uint32_t index;

	if (old_length < new_length)
	{
		/* allocate an area if addr is a NULL pointer */
		if (!addr) addr = pagingSearchArea(p, new_length);

		for (cur = (uint8_t *)addr + (old_length << PAGE_BITS); old_length < new_length; old_length++, cur += PAGE_SIZE)
		{
			index = physMemAllocPage(false);
			table = __getPagingEntry(p, cur, true);
			if (table->value)
			{
				/* we have a collision, before we can proceed we need to move everything to a new virtual memory location */
				new_addr = pagingSearchArea(p, new_length);
				addr     = __pagingMove(p, new_addr, addr, old_length);

				/* now update the current frame */
				cur = (uint8_t *)addr + (old_length << PAGE_BITS);

				/* and retry the operation */
				table = __getPagingEntry(p, cur, true);
				assert(!table->value);
			}

			/* reset */
			table->value	= 0;

			table->present	= 1;
			table->rw		= rw;
			table->user		= user;
			table->frame	= index;

			if (p == NULL) __flushTLBSingle(cur);
		}
	}
	else
	{
		for (cur = (uint8_t *)addr + (new_length << PAGE_BITS); new_length < old_length; old_length--, cur += PAGE_SIZE)
		{
			table = __getPagingEntry(p, cur, false);
			assert(table && table->value);

			if (!table->present)
			{
				switch (table->avail)
				{
					case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
						table->value = 0;
						if (p == NULL) __flushTLBSingle(cur);
						continue;

					case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
						physMemPageIn(table->frame);
						break;

					case PAGING_AVAIL_NOTPRESENT_RESERVED:
					default:
						assert(0);
				}

				assert(table->present);
			}

			/* reset */
			index = table->frame;
			table->value = 0;

			physMemReleasePage(index);

			if (p == NULL) __flushTLBSingle(cur);
		}

		/* return NULL if the memory pointer is now invalid */
		if (new_length == 0)
			addr = NULL;
	}

	return addr;
}

/**
 * @brief Releases several pages of physical memory in a process
 * @details This functions iterates through length pages starting from the base
 *			address and releases all associated physical memory. If the area contains
 *			pages which are not reserved an assertion is triggered, because this typically
 *			means that there is a programming error somewhere.
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param addr Address pointing to some allocated blocks of memory
 * @param length Number of consecutive pages to release
 */
void pagingReleasePhysMem(struct process *p, void *addr, uint32_t length)
{
	assert(pagingTryReleasePhysMem(p, addr, length));
}

/**
 * @brief Releases several pages of physical memory of a process
 * @details This functions iterates through length pages starting from the base
 *			address and releases all associated physical memory. If the area contains
 *			pages which are not present the return value is false.
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param addr Address pointing to some allocated blocks of memory
 * @param length Number of consecutive pages to release
 *
 * @return True if all pages were successfully unmapped, otherwise false
 */
bool pagingTryReleasePhysMem(struct process *p, void *addr, uint32_t length)
{
	struct pagingEntry *table;
	uint8_t *cur;
	uint32_t index;
	bool success = true;

	for (cur = addr; length; length--, cur += PAGE_SIZE)
	{
		table = __getPagingEntry(p, cur, false);
		if (!table || !table->value)
		{
			success = false;
			continue;
		}

		if (!table->present)
		{
			switch (table->avail)
			{
				case PAGING_AVAIL_NOTPRESENT_RESERVED:
				case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
					table->value = 0;
					if (p == NULL) __flushTLBSingle(cur);
					continue;

				case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
					physMemPageIn(table->frame);
					break;

				default:
					assert(0);
			}

			assert(table->present);
		}

		index = table->frame;
		table->value = 0;

		physMemReleasePage(index);

		if (p == NULL) __flushTLBSingle(cur);
	}

	return success;
}

/**
 * @brief Returns the physical page index for a virtual address
 * @details Looks up the virtual address in the page table of the corresponding
 *			process and returns the physical page index. If the page is paged out
 *			to the hard drive, then it is paged in again in this process. If the
 *			page is not mapped at all an internal assertion is triggered.
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param addr Address pointing to some allocated blocks of memory
 *
 * @return Physical page index
 */
uint32_t pagingGetPhysMem(struct process *p, void *addr)
{
	struct pagingEntry *table;

	table = __getPagingEntry(p, addr, false);
	assert(table && table->value);

	if (!table->present)
	{
		switch (table->avail)
		{
			case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
				physMemPageIn(table->frame);
				break;

			case PAGING_AVAIL_NOTPRESENT_RESERVED:
			case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
			default:
				assert(0);
		}

		assert(table->present);
	}

	return table->frame;
}

/**
 * @brief Maps some virtual memory from one process to another one
 *
 * @param dst_p Pointer to the destination process object or NULL
 * @param src_p Pointer to the source process object or NULL
 * @param dst_addr Address where the memory should be mapped or NULL
 * @param src_addr Virtual address of the source memory block
 * @param length Number of consecutive pages to map into the destination process
 * @param rw If true then the page has write permission, otherwise it is a read-only page
 * @param user If true then the user (ring3) also has access to the page, otherwise only the kernel
 * @return Virtual base address to the mapped memory block (inside of the destination process)
 */
void *pagingMapRemoteMemory(struct process *dst_p, struct process *src_p, void *dst_addr, void *src_addr, uint32_t length, bool rw, bool user)
{
	struct pagingEntry *src, *dst;
	uint8_t *src_cur, *dst_cur;

	if (!dst_addr)
		dst_addr = pagingSearchArea(dst_p, length);

	/* we don't allow mapping something in the NULL page for now */
	if (((uint32_t)dst_addr & ~PAGE_MASK) == 0) return NULL;

	/* reserve the whole area - this is necessary since we want to mark the
	 * memory as unpageable, which will probably again call a memory allocator */
	pagingReserveArea(dst_p, dst_addr, length, user);

	for (src_cur = src_addr, dst_cur = dst_addr; length; length--, src_cur += PAGE_SIZE, dst_cur += PAGE_SIZE)
	{
		src = __getPagingEntry(src_p, src_cur, false);
		assert(src && src->value);

		dst = __getPagingEntry(dst_p, dst_cur, true);
		assert(__isReserved(dst));

		if (!src->present)
		{
			switch (src->avail)
			{
				case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
					physMemPageIn(src->frame);
					break;

				case PAGING_AVAIL_NOTPRESENT_RESERVED:
				case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
				default:
					assert(0);
			}

			assert(src->present);
		}

		if (rw && !src->rw && src->avail == PAGING_AVAIL_PRESENT_ON_WRITE_DUPLICATE)
		{
			/* duplicate this page */
			src->rw		= 1;
			src->avail	= 0;

			if (!physMemIsLastRef(src->frame))
			{
				uint32_t old_index = src->frame;
				src->frame = physMemAllocPage(false);
				void *destination	= __pagingMapPhysMem(NULL, physMemAddRefPage(src->frame), NULL, true, false);
				void *source		= __pagingMapPhysMem(NULL, physMemAddRefPage(old_index), NULL, true, false);

				memcpy(destination, source, PAGE_SIZE);

				pagingReleasePhysMem(NULL, destination, 1);
				pagingReleasePhysMem(NULL, source, 1);
				physMemReleasePage(old_index);
			}
		}

		/* copy the whole entry to the destination */
		*dst = *src;

		/* adjust permissions */
		dst->rw			= rw;
		dst->user		= user;

		/* increase refcount */
		physMemAddRefPage(dst->frame);

		if (dst_p == NULL) __flushTLBSingle(dst_cur);
	}

	return (void *)((uint32_t)dst_addr | ((uint32_t)src_addr & PAGE_MASK));
}

/**
 * @brief Allocates the page directory and page table for a specific process
 * @details Each process needs its own page directory and page table such that
 *			they have no influence on each other. This command allocates a
 *			minimal page table which can then be used to map the rest of the
 *			memory into a new usermode process.
 *
 * @param p Pointer to a process object
 */
void pagingAllocProcessPageTable(struct process *p)
{
	bool pagingEnabled	= (__getCR0() & 0x80000000);
	uint32_t i;

	assert(pagingEnabled && p != NULL);
	assert(p->pageDirectory == NULL);

	p->pageDirectory		= pagingAllocatePhysMem(NULL, 1, true, false);
	memset(p->pageDirectory, 0, PAGE_SIZE);

	for (i = 0; i < PAGETABLE_COUNT; i++)
		p->pageTables[i] = NULL;
}

/**
 * @brief Duplicate a page table and assigns it to a destination process
 * @details During the fork syscall it is necessary initialize the child process
 *			with exactly the same memory layout as the parent process. To make this
 *			possible we copy the page directory and page table. All other pages
 *			are marked as read-only, so they will be copied later when it is necessary.
 *
 * @param destination Pointer to the destination process object (child)
 * @param source Pointer to the source process object (parent)
 */
void pagingForkProcessPageTable(struct process *destination, struct process *source)
{
	bool pagingEnabled	= (__getCR0() & 0x80000000);
	struct pagingEntry *src, *dst;
	uint32_t i;

	assert(pagingEnabled && destination != NULL && source != NULL);
	assert(source->pageDirectory != NULL);
	assert(destination->pageDirectory == NULL);

	destination->pageDirectory	= pagingAllocatePhysMem(NULL, 1, true, false);
	memset(destination->pageDirectory, 0, PAGE_SIZE);

	for (i = 0; i < PAGETABLE_COUNT; i++)
		destination->pageTables[i] = NULL;

	/* TODO: Make this more efficient */
	for (i = 0; i < PAGETABLE_COUNT * PAGETABLE_COUNT; i++)
	{
		src = __getPagingEntry(source, (void *)(i << PAGE_BITS), false);
		if (!src)
		{
			i |= PAGETABLE_MASK;
		}
		else if (src->value)
		{
			dst = __getPagingEntry(destination, (void *)(i << PAGE_BITS), true);
			assert(!dst->value);

			if (!src->present)
			{
				switch (src->avail)
				{
					case PAGING_AVAIL_NOTPRESENT_RESERVED:
					case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
						*dst = *src;
						break;

					case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
						physMemPageIn(src->frame);
						break;

					default:
						assert(0);
				}

				assert(src->present);
			}

			switch (src->avail)
			{
				case 0:
					if (src->rw)
					{
						src->rw = 0;
						src->avail = PAGING_AVAIL_PRESENT_ON_WRITE_DUPLICATE;
					}
					*dst = *src;
					physMemAddRefPage(dst->frame);
					break;

				case PAGING_AVAIL_PRESENT_SHARED:
				case PAGING_AVAIL_PRESENT_ON_WRITE_DUPLICATE:
					*dst = *src;
					physMemAddRefPage(dst->frame);
					break;

				case PAGING_AVAIL_PRESENT_NO_FORK:
					break;

				default:
					assert(0);
			}
		}
	}
}

/**
 * @brief Releases the page directory and page table of a specific process
 * @details Deallocates the memory used for the page directory and page able.
 *
 * @param p Pointer to the process object
 */
void pagingReleaseProcessPageTable(struct process *p)
{
	bool pagingEnabled	= (__getCR0() & 0x80000000);
	struct pagingEntry *table;
	uint32_t index, i;

	assert(pagingEnabled && p != NULL);
	assert(p->pageDirectory);

	/* TODO: Make this more efficient */
	for (i = 0; i < PAGETABLE_COUNT * PAGETABLE_COUNT; i++)
	{
		table = __getPagingEntry(p, (void *)(i << PAGE_BITS), false);
		if (!table)
		{
			i |= PAGETABLE_MASK;
		}
		else if (table->value)
		{

			if (!table->present)
			{
				switch (table->avail)
				{
					case PAGING_AVAIL_NOTPRESENT_RESERVED:
					case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
						continue;

					case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
						physMemPageIn(table->frame);
						break;

					default:
						assert(0);
				}

				assert(table->present);
			}

			/* reset */
			index = table->frame;
			table->value = 0;

			physMemReleasePage(index);
		}
	}

	for (i = 0; i < PAGETABLE_COUNT; i++)
	{
		if (p->pageTables[i])
		{
			pagingReleasePhysMem(NULL, p->pageTables[i], 1);
			p->pageTables[i] = NULL;
		}

		if (p->pageDirectory[i].value)
		{

			if (!p->pageDirectory[i].present)
			{
				switch (p->pageDirectory[i].avail)
				{
					case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
						physMemPageIn(p->pageDirectory[i].frame);
						break;

					case PAGING_AVAIL_NOTPRESENT_RESERVED:
					case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
					default:
						assert(0);
				}

				assert(p->pageDirectory[i].present);
			}

			index = p->pageDirectory[i].frame;
			p->pageDirectory[i].value = 0;

			physMemReleasePage(index);
		}
	}

	pagingReleasePhysMem(NULL, p->pageDirectory, 1);
	p->pageDirectory = NULL;
}

/**
 * @brief Fills out all memory related fields in the processInfo structure
 *
 * @param p Pointer to the process object
 * @param info Pointer to the processInfo structure
 */
void pagingFillProcessInfo(struct process *p, struct processInfo *info)
{
	bool pagingEnabled	= (__getCR0() & 0x80000000);
	struct pagingEntry *table;
	uint32_t i;

	assert(pagingEnabled);
	assert(info);

	/* reset memory fields */
	info->pagesPhysical		= 0;
	info->pagesShared		= 0;
	info->pagesNoFork		= 0;
	info->pagesReserved		= 0;
	info->pagesOutpaged		= 0;

	/* no page directory found */
	if (p && !p->pageDirectory) return;

	/* TODO: Make this more efficient */
	for (i = 0; i < PAGETABLE_COUNT * PAGETABLE_COUNT; i++)
	{
		table = __getPagingEntry(p, (void *)(i << PAGE_BITS), false);
		if (!table)
		{
			i |= PAGETABLE_MASK;
		}
		else if (table->value)
		{
			if (table->present)
			{
				switch (table->avail)
				{
					case 0:
						info->pagesPhysical++;
						break;

					case PAGING_AVAIL_PRESENT_SHARED:
					case PAGING_AVAIL_PRESENT_ON_WRITE_DUPLICATE:
						info->pagesShared++;
						break;

					case PAGING_AVAIL_PRESENT_NO_FORK:
						info->pagesNoFork++;
						break;
				}

			}
			else
			{
				switch (table->avail)
				{
					case PAGING_AVAIL_NOTPRESENT_RESERVED:
					case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
						info->pagesReserved++;
						break;

					case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
						info->pagesOutpaged++;
						break;

					default:
						assert(0);
				}

			}
		}
	}

}

/**
 * @brief Maps some virtual memory of a usermode process into the kernel
 * @details This function is similar to pagingMapRemoteMemory(), but will never trigger
 *			a system failure, even if the provided arguments are invalid (page not mapped
 *			or wrong permissions). If it is not possible to map the whole area of length
 *			pages, then the whole area will be unmapped again.
 *
 * @param src_p Pointer to the process object
 * @param src_addr Virtual address of the source memory block
 * @param length Number of consecutive pages to map into the kernel
 * @param rw If true then the page has write permission, otherwise it is a read-only page
 * @return Virtual base address to the mapped memory block (inside of the kernel)
 */
void *pagingTryMapUserMem(struct process *src_p, void *src_addr, uint32_t length, bool rw)
{
	struct pagingEntry *src, *dst;
	uint8_t *src_cur, *dst_cur;
	void *dst_addr = pagingSearchArea(NULL, length);

	/* reserve the whole area - this is necessary since we want to mark the
	 * memory as unpageable, which will probably again call a memory allocator */
	pagingReserveArea(NULL, dst_addr, length, false);

	for (src_cur = src_addr, dst_cur = dst_addr; length; length--, src_cur += PAGE_SIZE, dst_cur += PAGE_SIZE)
	{
		src = __getPagingEntry(src_p, src_cur, false);
		if (!src || !src->value || !src->user) goto invalid;

		dst = __getPagingEntry(NULL, dst_cur, true);
		assert(__isReserved(dst));

		if (!src->present)
		{
			switch (src->avail)
			{
				case PAGING_AVAIL_NOTPRESENT_RESERVED:
					goto invalid;

				case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
					physMemPageIn(src->frame);
					break;

				case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
				default:
					assert(0);
			}

			assert(src->present);
		}


		if (rw && !src->rw)
		{
			if (src->avail != PAGING_AVAIL_PRESENT_ON_WRITE_DUPLICATE)
				goto invalid;

			/* duplicate this page */
			src->rw		= 1;
			src->avail	= 0;

			if (!physMemIsLastRef(src->frame))
			{
				uint32_t old_index = src->frame;
				src->frame = physMemAllocPage(false);
				void *destination	= __pagingMapPhysMem(NULL, physMemAddRefPage(src->frame), NULL, true, false);
				void *source		= __pagingMapPhysMem(NULL, physMemAddRefPage(old_index), NULL, true, false);

				memcpy(destination, source, PAGE_SIZE);

				pagingReleasePhysMem(NULL, destination, 1);
				pagingReleasePhysMem(NULL, source, 1);
				physMemReleasePage(old_index);
			}
		}

		/* copy the whole entry to the destination */
		*dst = *src;

		/* adjust permissions */
		dst->rw			= rw;
		dst->user		= false;

		/* increase refcount */
		physMemAddRefPage(dst->frame);

		__flushTLBSingle(dst_cur);
	}

	return (void *)((uint32_t)dst_addr | ((uint32_t)src_addr & PAGE_MASK));

invalid:
	/* exploit attempt, pointer is not valid in user space for ring3 */
	pagingReleasePhysMem(NULL, dst_addr, (((uint32_t)dst_cur - (uint32_t)dst_addr) >> PAGE_BITS) + length);
	return NULL;
}

/**
 * @brief Releases several pages of physical memory of a process
 * @details This function is similar to pagingTryReleasePhysMem() and iterates
 *			through length pages starting from the base address. It releases all
 *			associated physical memory, but only if the pages are accessible for
 *			the usermode process.
 *
 * @param p Pointer to a process object or NULL for the kernel
 * @param addr Address pointing to some allocated blocks of memory
 * @param length Number of consecutive pages to release
 * @return True on success (all pages were valid and owned by the user), otherwise false
 */
bool pagingTryReleaseUserMem(struct process *p, void *addr, uint32_t length)
{
	struct pagingEntry *table;
	uint8_t *cur;
	uint32_t index;
	bool success = true;

	for (cur = addr; length; length--, cur += PAGE_SIZE)
	{
		table = __getPagingEntry(p, cur, false);
		if (!table || !table->value || !table->user)
		{
			success = false;
			continue;
		}

		if (!table->present)
		{
			switch (table->avail)
			{
				case PAGING_AVAIL_NOTPRESENT_RESERVED:
				case PAGING_AVAIL_NOTPRESENT_ON_ACCESS_CREATE:
					table->value = 0;
					if (p == NULL) __flushTLBSingle(cur);
					continue;

				case PAGING_AVAIL_NOTPRESENT_OUTPAGED:
					physMemPageIn(table->frame);
					break;

				default:
					assert(0);
			}

			assert(table->present);
		}

		/* reset */
		index = table->frame;
		table->value = 0;

		physMemReleasePage(index);

		if (p == NULL) __flushTLBSingle(cur);
	}

	return success;
}
