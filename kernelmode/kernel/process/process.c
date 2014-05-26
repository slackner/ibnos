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

#include <process/process.h>
#include <process/handle.h>
#include <process/thread.h>
#include <process/object.h>
#include <hardware/gdt.h>
#include <memory/paging.h>
#include <memory/allocator.h>
#include <util/list.h>
#include <util/util.h>

/** \addtogroup Processes
 *  @{
 *	Implementation of Processes
 */

struct linkedList processList = LL_INIT(processList);

static void __processDestroy(struct object *obj);
static void __processShutdown(struct object *obj, uint32_t mode);
static int32_t __processGetStatus(struct object *obj, UNUSED uint32_t mode);
static struct linkedList *__processWait(struct object *obj, UNUSED uint32_t mode, uint32_t *result);

static const struct objectFunctions processFunctions =
{
	__processDestroy,
	NULL, /* getMinHandle */
	__processShutdown,
	__processGetStatus,
	__processWait,
	NULL, /* signal */
	NULL, /* write */
	NULL, /* read */
	NULL, /* insert */
	NULL, /* remove */
};

/**
 * @brief Creates a new kernel process object
 * @details This function allocates and initializes the structure used to store
 *			a process. If original is NULL then the new process will have an empty
 *			minimal memory layout and empty handle table, otherwise all information
 *			will be duplicated from the original process. The process be initialized
 *			with a refcount of 1, but each thread inside of it increases the refcount.
 *			This allows processes to exist even if there is nothing externally pointing
 *			to them.
 *
 * @param original Pointer to the original process object or NULL
 * @return Pointer to the new kernel process object
 */
struct process *processCreate(struct process *original)
{
	struct process *p;

	/* allocate some new memory */
	if (!(p = heapAlloc(sizeof(*p))))
		return NULL;

	/* initialize general object info */
	__objectInit(&p->obj, &processFunctions);
	ll_init(&p->waiters);
	ll_add_tail(&processList, &p->entry_list);
	p->exitcode = -1;
	ll_init(&p->threads);
	p->pageDirectory = NULL;
	p->entryPoint    = NULL;

	/* initialize paging for the new process */
	if (!original)
	{
		handleTableInit(&p->handles);
		pagingAllocProcessPageTable(p);

		pagingMapRemoteMemory(p, NULL, (void *)USERMODE_KERNELSTACK_ADDRESS, kernelStack, 1, true, false);							/* kernelstack */
		pagingMapRemoteMemory(p, NULL, (void *)USERMODE_GDT_ADDRESS, (void *)USERMODE_GDT_ADDRESS, GDT_MAX_PAGES, false, false);	/* gdt */
		pagingMapRemoteMemory(p, NULL, (void *)USERMODE_IDT_ADDRESS, (void *)USERMODE_IDT_ADDRESS, 1, false, false);				/* idt */
		pagingMapRemoteMemory(p, NULL, (void *)USERMODE_INTJMP_ADDRESS, intJmpTable_user, 1, false, false);							/* intjmp */
		pagingMapRemoteMemory(p, NULL, (void *)USERMODE_TASK_ADDRESS, (void *)USERMODE_TASK_ADDRESS, 1, false, false);				/* task */
	}
	else
	{
		handleForkTable(&p->handles, &original->handles);
		pagingForkProcessPageTable(p, original);
	}

	p->user_programArgumentsBase		= NULL;
	p->user_programArgumentsLength		= 0;
	p->user_environmentVariablesBase	= NULL;
	p->user_environmentVariablesLength	= 0;

	return p;
}

/**
 * @brief Destructor for kernel process objects
 * @details This function also releases the page table and all handles which are
 *			part of the process structure.
 *
 * @param obj Pointer to the kernel process object
 */
static void __processDestroy(struct object *obj)
{
	struct process *p = objectContainer(obj, struct process, &processFunctions);

	/* there should be no more threads waiting on this process */
	assert(ll_empty(&p->waiters));

	/* list of threads should be empty at this point */
	assert(ll_empty(&p->threads));

	/* free paging table (if not done yet) */
	if (p->pageDirectory)
	{
		pagingReleaseProcessPageTable(p);
		p->pageDirectory = NULL;
	}

	/* free opened handles */
	if (p->handles.handles)
	{
		handleTableFree(&p->handles);
		p->handles.handles = NULL;
	}

	/* unlink from list of existing processes */
	ll_remove(&p->entry_list);

	/* release process structure memory */
	p->obj.functions = NULL;
	heapFree(p);
}

/**
 * @brief Forces termination of a kernel process object
 * @details This function first terminates all threads and afterwards releases
 *			the memory of the page table and handles.
 *
 * @param obj Pointer to the kernel process object
 * @param exitcode Exitcode which can be queried later by other processes
 */
static void __processShutdown(struct object *obj, uint32_t exitcode)
{
	struct process *p = objectContainer(obj, struct process, &processFunctions);
	struct thread *t, *__t;

	p->exitcode = exitcode;

	/* required to allow shutting down processes using borrowed references */
	objectAddRef(p);

	/* wake up waiting threads */
	queueWakeup(&p->waiters, true, p->exitcode);

	/* shutdown all other threads */
	LL_FOR_EACH_SAFE(t, __t, &p->threads, struct thread, entry_process)
	{
		objectShutdown(t, p->exitcode);
	}

	/* free paging table */
	if (p->pageDirectory)
	{
		pagingReleaseProcessPageTable(p);
		p->pageDirectory = NULL;
	}

	/* free opened handles */
	if (p->handles.handles)
	{
		handleTableFree(&p->handles);
		p->handles.handles = NULL;
	}

	objectRelease(p);
}

/**
 * @brief Returns the process exitcode or (-1) if the process was not yet terminated
 * @details This function will return the exitcode if the process was already
 *			terminated, otherwise (-1) will be returned. To find out if the
 *			process was terminated the recommended method is to use wait, which will
 *			only return when this is the case.
 *
 * @param obj Pointer to the kernel process object
 * @param mode not used
 *
 * @return Exitcode of the process or (-1)
 */
static int32_t __processGetStatus(struct object *obj, UNUSED uint32_t mode)
{
	struct process *p = objectContainer(obj, struct process, &processFunctions);
	return ll_empty(&p->threads) ? (signed)p->exitcode : (-1);
}

/**
 * @brief Wait until the kernel process object is terminated
 * @details If the process is already terminated this function will return
 *			immediately with the exitcode. Otherwise the wait queue linked list
 *			is returned and the call will be blocking.
 *
 * @param obj Pointer to the kernel process object
 * @param mode not used
 * @param result Will be filled out with the result code if the operation doesn't block
 * @return NULL if the operation returns immediately, otherwise a pointer to a wait queue linked list
 */
static struct linkedList *__processWait(struct object *obj, UNUSED uint32_t mode, uint32_t *result)
{
	struct process *p = objectContainer(obj, struct process, &processFunctions);
	if (!ll_empty(&p->threads)) return &p->waiters;

	*result = p->exitcode;
	return NULL;
}

/**
 * @brief Returns the number of total processes in the system
 * @return Number of processes
 */
uint32_t processCount()
{
	struct process *p;
	uint32_t count = 0;
	LL_FOR_EACH(p, &processList, struct process, entry_list)
	{
		/* just increase counter */
		count++;
	}
	return count;
}

/**
 * @brief Fills out the processInfo structure with information about each individual process
 *
 * @param info Array where the output should be stored
 * @param count Length of the buffer in sizes of processInfo
 *
 * @return Number of process which have been stored
 */
uint32_t processInfo(struct processInfo *info, uint32_t count)
{
	uint32_t processCount = 0;
	struct process *p;
	struct thread *t;

	/* kernel itself */
	if (count)
	{
		/* let the paging module fill out all memory related fields */
		pagingFillProcessInfo(NULL, info);

		/* fill out all other fields */
		info->processID					= 0;
		info->handleCount				= 0;
		info->numberOfTotalThreads		= 0;
		info->numberOfBlockedThreads	= 0;

		count--;
		info++;
	}

	LL_FOR_EACH(p, &processList, struct process, entry_list)
	{
		if (count)
		{
			/* let the paging module fill out all memory related fields */
			pagingFillProcessInfo(p, info);

			/* fill out all other fields */
			info->processID					= (uint32_t)p; /* FIXME: associate some other identifiers? */
			info->handleCount				= p->handles.handles ? handleCount(&p->handles) : 0;
			info->numberOfTotalThreads		= 0;
			info->numberOfBlockedThreads	= 0;

			LL_FOR_EACH(t, &p->threads, struct thread, entry_process)
			{
				info->numberOfTotalThreads++;
				if (t->blocked) info->numberOfBlockedThreads++;
			}

			count--;
			info++;
		}

		processCount++;
	}

	return processCount;
}

/**
 * @}
 */
