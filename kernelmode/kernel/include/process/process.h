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

#ifndef _H_PROCESS_
#define _H_PROCESS_

#include <stdint.h>

struct processInfo
{
	uint32_t processID;

	uint32_t pagesPhysical;
	uint32_t pagesShared;
	uint32_t pagesNoFork;
	uint32_t pagesReserved;
	uint32_t pagesOutpaged;

	uint32_t handleCount;
	uint32_t numberOfTotalThreads;
	uint32_t numberOfBlockedThreads;
};

#ifdef __KERNEL__

	struct process;

	#include <stdbool.h>

	#include <process/object.h>
	#include <process/handle.h>
	#include <memory/physmem.h>
	#include <memory/paging.h>
	#include <util/list.h>

	extern struct linkedList processList;

	struct process
	{
		struct object obj;
		struct linkedList waiters;

		/* entry in the processList */
		struct linkedList entry_list;
		uint32_t exitcode;

		/* list of threads associated to this process */
		struct linkedList threads;

		/* page directory and page tables (mapped into the kernel) */
		struct pagingEntry *pageDirectory;
		struct pagingEntry *pageTables[PAGETABLE_COUNT];

		/* entryPoint of main thread */
		void *entryPoint;

		/* handles */
		struct handleTable handles;
	};

	struct process *processCreate(struct process *original);
	uint32_t processCount();
	uint32_t processInfo(struct processInfo *info, uint32_t count);

#endif

#endif /* _H_PROCESS_ */