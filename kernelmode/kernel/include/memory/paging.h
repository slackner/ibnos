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

#ifndef _H_PAGING_
#define _H_PAGING_

/** \addtogroup Paging
 *  @{
 */

#define PAGETABLE_SIZE 0x1000
#define PAGETABLE_MASK 0x3FF
#define PAGETABLE_BITS 10
#define PAGETABLE_COUNT 0x400

#ifdef __KERNEL__

	#include <stddef.h>
	#include <stdint.h>
	#include <stdbool.h>

	#include <process/thread.h>
	#include <process/process.h>

	struct pagingEntry
	{
		union
		{
			struct
			{
				uint32_t present		: 1;
				uint32_t rw				: 1;
				uint32_t user			: 1;
				uint32_t __res1			: 2;
				uint32_t dirty			: 1;
				uint32_t accessed		: 1;
				uint32_t __res2			: 2;
				uint32_t avail			: 3;
				uint32_t frame			: 20;
			};
			uint32_t value;
		};
	} __attribute__((packed));

	/**
	 * \ingroup Interrupts
	 */
	uint32_t interrupt_0x0E(uint32_t interrupt, uint32_t error, struct thread *t);

	/* internally used by physmem.c */
	void pagingInsertBootMap(uint32_t startIndex, uint32_t stopIndex);
	void pagingDumpBootMap();

	void pagingInit();
	void pagingDumpPageTable(struct process *p);

	void pagingReserveArea(struct process *p, void *addr, uint32_t length, bool user);
	void *pagingSearchArea(struct process *p, uint32_t length);
	void *pagingTrySearchArea(struct process *p, uint32_t length);

	void *pagingAllocatePhysMem(struct process *p, uint32_t length, bool rw, bool user);
	void *pagingAllocatePhysMemUnpageable(struct process *p, uint32_t length, bool rw, bool user);
	void *pagingTryAllocatePhysMem(struct process *p, uint32_t length, bool rw, bool user);

	void *pagingAllocatePhysMemFixed(struct process *p, void *addr, uint32_t length, bool rw, bool user);
	void *pagingAllocatePhysMemFixedUnpageable(struct process *p, void *addr, uint32_t length, bool rw, bool user);
	void *pagingTryAllocatePhysMemFixed(struct process *p, void *addr, uint32_t length, bool rw, bool user);

	void *pagingReAllocatePhysMem(struct process *p, void *addr, uint32_t old_length, uint32_t new_length, bool rw, bool user);

	void pagingReleasePhysMem(struct process *p, void *addr, uint32_t length);
	bool pagingTryReleasePhysMem(struct process *p, void *addr, uint32_t length);
	bool pagingTryReleaseUserMem(struct process *p, void *addr, uint32_t length);

	uint32_t pagingGetPhysMem(struct process *p, void *addr);

	void *pagingMapRemoteMemory(struct process *dst_p, struct process *src_p, void *dst_addr, void *src_addr, uint32_t length, bool rw, bool user);
	void *pagingTryMapUserMem(struct process *src_p, void *src_addr, uint32_t length, bool rw);

	void pagingAllocProcessPageTable(struct process *p);
	void pagingForkProcessPageTable(struct process *destination, struct process *source);
	void pagingReleaseProcessPageTable(struct process *p);
	void pagingFillProcessInfo(struct process *p, struct processInfo *info);

	/* macros to simplify user memory access */
	struct userMemory
	{
		void *addr;			/* virtual address in kernel process */
		uint32_t length;	/* length in pages */
	};

	static inline bool ACCESS_USER_MEMORY(struct userMemory *k, struct process *p, void *src_addr, uint32_t byte_length, bool rw)
	{
		if (!byte_length)
		{
			k->length	= 0;
			k->addr		= NULL;
			return true;
		}

		k->length	= (((uint32_t)src_addr & PAGE_MASK) + byte_length + PAGE_MASK) >> PAGE_BITS;
		k->addr		= pagingTryMapUserMem(p, src_addr, k->length, rw);
		return (k->addr != NULL);
	}

	static inline bool ACCESS_USER_MEMORY_STRUCT(struct userMemory *k, struct process *p, void *src_addr, uint32_t count, uint32_t struct_length, bool rw)
	{
		uint64_t byte_length = (uint64_t)count * struct_length;
		if ((byte_length >> 32) != 0) return false;
		return ACCESS_USER_MEMORY(k, p, src_addr, byte_length, rw);
	}

	static inline void RELEASE_USER_MEMORY(struct userMemory *k)
	{
		if (k->addr) pagingReleasePhysMem(NULL, k->addr, k->length);
	}

#endif
/**
 *  @}
 */
#endif /* _H_PAGING_ */