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

#include <process/thread.h>
#include <process/process.h>
#include <process/object.h>
#include <hardware/gdt.h>
#include <interrupt/interrupt.h>
#include <memory/physmem.h>
#include <memory/paging.h>
#include <memory/allocator.h>
#include <util/list.h>
#include <util/util.h>

#define DEFAULT_STACK_SIZE	0x10000
#define DEFAULT_TLB_SIZE	0x1000

/** \addtogroup Threads
 *  @{
 *  Implementation of threads.
 */

struct linkedList threadList = LL_INIT(threadList);
struct thread *lastFPUthread;

static void __threadDestroy(struct object *obj);
static void __threadShutdown(struct object *obj, uint32_t mode);
static int32_t __threadGetStatus(struct object *obj, UNUSED uint32_t mode);
static struct linkedList *__threadWait(struct object *obj, UNUSED uint32_t mode, uint32_t *result);
static void __threadSignal(struct object *obj, uint32_t result);

static const struct objectFunctions threadFunctions =
{
	__threadDestroy,
	NULL, /* getMinHandle */
	__threadShutdown,
	__threadGetStatus,
	__threadWait,
	__threadSignal,
	NULL, /* write */
	NULL, /* read */
	NULL, /* insert */
	NULL, /* remove */
};

/**
 * @brief Creates a new kernel thread object
 * @details This function allocates and initializes the structure used to store
 *			a thread. If original is NULL then all task registers will be initialized
 *			to their default values, otherwise they will be duplicated from the original
 *			thread. When duplicating an existing thread all the additional arguments
 *			like the entry point are ignored.
 *
 * @param p Pointer to the kernel process object which should contain the new thread
 * @param original Pointer to the original thread object or NULL
 * @param eip Entrypoint (EIP register) of the newly created process
 * @return Pointer to the thread kernel object
 */
struct thread *threadCreate(struct process *p, struct thread *original, void *eip)
{
	struct thread *t;
	struct taskContext *task;;
	assert(p);

	/* allocate some new memory */
	if (!(t = heapAlloc(sizeof(*t))))
		return NULL;

	/* initialize general object info */
	__objectInit(&t->obj, &threadFunctions);
	ll_add_tail(&threadList, &t->obj.entry);
	ll_init(&t->waiters);
	t->blocked = false;
	t->process = p;
	ll_add_tail(&p->threads, &t->entry_process);
	t->exitcode = -1;

	if (!original)
	{
		t->fpuInitialized = false;

		t->user_ring3StackLength	= DEFAULT_STACK_SIZE >> PAGE_BITS;
		t->user_ring3StackBase		= pagingAllocatePhysMem(p, t->user_ring3StackLength, true, true);

		t->user_threadLocalLength	= DEFAULT_TLB_SIZE >> PAGE_BITS;
		t->user_threadLocalBase		= pagingAllocatePhysMem(p, t->user_threadLocalLength, true, true);

		/* initialize the cpu registers */
		task = &t->task;
		memset(task, 0, sizeof(*task));
		task->prevTask	= 0;
		task->esp0		= USERMODE_KERNELSTACK_LIMIT;
		task->ss0		= gdtGetEntryOffset(dataRing0, GDT_CPL_RING0);
		task->esp1		= 0;
		task->ss1		= 0;
		task->esp2		= 0;
		task->ss2		= 0;
		task->cr3		= pagingGetPhysMem(NULL, p->pageDirectory) << PAGE_BITS;
		task->eip		= (uint32_t)eip;
		task->eflags	= (1 << 9); /* Enable interrupts */

		task->eax		= 0;
		task->ecx		= 0;
		task->edx		= 0;
		task->ebx		= 0;
		task->esp		= (uint32_t)t->user_ring3StackBase + (t->user_ring3StackLength << PAGE_BITS);
		task->ebp		= 0;
		task->esi		= 0;
		task->edi		= 0;
		task->es		= gdtGetEntryOffset(dataRing3, GDT_CPL_RING3);
		task->cs		= gdtGetEntryOffset(codeRing3, GDT_CPL_RING3);
		task->ss		= gdtGetEntryOffset(dataRing3, GDT_CPL_RING3);
		task->ds		= gdtGetEntryOffset(dataRing3, GDT_CPL_RING3);
		task->fs		= gdtGetEntryOffset(dataRing3, GDT_CPL_RING3);
		task->gs		= gdtGetEntryOffset(dataRing3, GDT_CPL_RING3);
		task->ldt		= 0;
		task->iomap		= sizeof(*task);

	}
	else
	{
		/* forking a thread in the same process will not work! */
		assert(original->process && p != original->process);

		t->fpuInitialized = original->fpuInitialized;

		t->user_ring3StackLength	= original->user_ring3StackLength;
		t->user_ring3StackBase		= original->user_ring3StackBase;

		t->user_threadLocalLength	= original->user_threadLocalLength;
		t->user_threadLocalBase		= original->user_threadLocalBase;

		/* initialize the cpu registers */
		t->task			= original->task;
		t->task.cr3		= pagingGetPhysMem(NULL, p->pageDirectory) << PAGE_BITS;

		if (t->fpuInitialized)
		{
			/* we have to save the last fpu context to memory in order to make sure we copy the latest state */
			if (lastFPUthread == original)
			{
				asm volatile("clts");
				asm volatile("fnsave %0; fwait" : "=m" (lastFPUthread->fpu));
			}

			t->fpu = original->fpu;
		}
	}

	/* addref for the corresponding process and thread */
	objectAddRef(t);
	objectAddRef(t->process);

	return t;
}

/**
 * @brief Destructor for thread kernel objects
 *
 * @param obj Pointer to the kernel thread object
 */
static void __threadDestroy(struct object *obj)
{
	struct thread *t = objectContainer(obj, struct thread, &threadFunctions);
	if (t == lastFPUthread) lastFPUthread = NULL;

	/* there should be no more threads waiting on this one */
	assert(ll_empty(&t->waiters));

	/* thread should be disassociated from thread */
	assert(!t->process);

	/* release thread memory */
	t->obj.functions = NULL;
	heapFree(t);
}

/**
 * @brief Forces termination of the kernel thread object
 * @details This function terminates the current threadand afterwards releases
 *			the thread stack and thread local storage memory block. The handles are
 *			part of the process and will remain until the process itself is
 *			terminated (or the usermode process destroys them).
 *
 * @param obj Pointer to the kernel thread object
 * @param exitcode Exitcode which can be queried later by other threads
 */
static void __threadShutdown(struct object *obj, uint32_t exitcode)
{
	struct thread *t = objectContainer(obj, struct thread, &threadFunctions);
	struct process *p = t->process;
	if (t == lastFPUthread) lastFPUthread = NULL;

	if (t->process)
	{
		t->process  = NULL;
		t->exitcode = exitcode;

		assert(t->user_threadLocalBase);
		assert(t->user_ring3StackBase);

		/* release stacks */
		pagingReleasePhysMem(p, t->user_threadLocalBase, t->user_threadLocalLength);
		t->user_threadLocalBase = NULL;

		pagingReleasePhysMem(p, t->user_ring3StackBase, t->user_ring3StackLength);
		t->user_ring3StackBase = NULL;

		ll_remove(&t->obj.entry);
		ll_remove(&t->entry_process);

		/* wake up waiting threads */
		queueWakeup(&t->waiters, true, t->exitcode);

		/* special case: all threads termined, but nothing set the exit code */
		if (ll_empty(&p->threads))
			objectShutdown(p, t->exitcode);

		/* release objects */
		objectRelease(t);
		objectRelease(p);
	}
}

/**
 * @brief Returns the thread exitcode or (-1) if the thread was not yet terminated
 * @details This function will return the exitcode if the thread was already
 *			terminated, otherwise (-1) will be returned. To find out if the
 *			thread was terminated the recommended method is to use wait, which will
 *			only return when this is the case.
 *
 * @param obj Pointer to the kernel thread object
 * @param mode not used
 *
 * @return Exitcode of the thread or (-1)
 */
static int32_t __threadGetStatus(struct object *obj, UNUSED uint32_t mode)
{
	struct thread *t = objectContainer(obj, struct thread, &threadFunctions);
	return (t->process != NULL) ? (-1) : (signed)t->exitcode;
}

/**
 * @brief Wait until the kernel thread object is terminated
 * @details If the thread is already terminated this function will return
 *			immediately with the exitcode. Otherwise the wait queue linked list
 *			is returned and the call will be blocking.
 *
 * @param obj Pointer to the kernel thread object
 * @param mode not used
 * @param result Will be filled out with the result code if the operation doesn't block
 * @return NULL if the operation returns immediately, otherwise a pointer to a wait queue linked list
 */
static struct linkedList *__threadWait(struct object *obj, UNUSED uint32_t mode, uint32_t *result)
{
	struct thread *t = objectContainer(obj, struct thread, &threadFunctions);
	if (t->process != NULL) return &t->waiters;

	*result = t->exitcode;
	return NULL;
}

/**
 * @brief Wake up a blocked kernel thread object
 * @details This function is used internally to wake up thread objects after
 *			they have finished a blocking operation, but it can also be used by
 *			usermode applications - nevertheless this can be dangerous, so only
 *			do it if you're sure about what happens internally. The thread will
 *			resume execution with the given result value.
 *
 * @param obj Pointer to the kernel thread object
 * @param result Result of the wait operation
 */
static void __threadSignal(struct object *obj, uint32_t result)
{
	struct thread *t = objectContainer(obj, struct thread, &threadFunctions);
	if (!t->process || !t->blocked) return;

	t->blocked = false;
	t->task.eax = result;
	ll_remove(&t->obj.entry);
	ll_add_tail(&threadList, &t->obj.entry);
}

/**
 * @brief Runs a specific thread until it is terminated or the scheduler triggers the next thread
 *
 * @param t Pointer to the kernel thread object
 * @return Pointer to the next kernel thread object which should be executed
 */
static struct thread *__threadRun(struct thread *t)
{
	struct thread *next_t = NULL;
	struct process *p = t->process;
	uint32_t status = INTERRUPT_CONTINUE_EXECUTION;

	/* get next thread in the linked list */
	next_t = LL_ENTRY(t->obj.entry.next, struct thread, obj.entry);

	if (p)
	{
		/* run task and dispatch the interrupt */
		while (status == INTERRUPT_CONTINUE_EXECUTION)
		{
			assert(t->process == p && !t->blocked);
			status = tssRunUsermodeThread(t);
		}
	}

	/* handle special cases */
	if (status == INTERRUPT_EXIT_THREAD)
	{
		/* shutdown the current thread (using the borrowed reference) */
		objectShutdown(t, t->task.ebx);
	}
	else if (status == INTERRUPT_EXIT_PROCESS)
	{
		/* get pointer to the next process */
		while (&next_t->obj.entry != &threadList && next_t->process == p)
			next_t = LL_ENTRY(next_t->obj.entry.next, struct thread, obj.entry);

		/* terminate the current process (using the borrowed reference) */
		objectShutdown(p, t->task.ebx);
	}

	return next_t;
}

/**
 * @brief Schedules threads until all process have been terminated
 * @details This is the main function which is responsible for running usermode
 *			code. It will be blocking until all processes have been terminated.
 */
void threadSchedule()
{
	struct thread *t;

	/* if the last process is terminated there is nothing we can do */
	while (!ll_empty(&processList))
	{

		/* as long as threads are available schedule them one after each other */
		while (!ll_empty(&threadList))
		{
			/* we automatically get a pointer to the next thread which should be executed */
			for (t = LL_ENTRY(threadList.next, struct thread, obj.entry); &t->obj.entry != &threadList;)
				t = __threadRun(t);
		}

		/* enable interrupts and wait */
		tssKernelIdle();
	}
}

/**
 * @brief Makes a kernel thread object wait for some waitable object
 * @details This function first checks if the wait object will be blocking, or if
 *			the operation continues immediately. If it is blocking it will
 *			unlink the thread from the list of runnable threads, and append it to
 *			the wait queue for the corresponding wait object.
 *
 * @param t Pointer to the kernel thread object
 * @param obj Wait object
 * @param mode Mode of the wait operation
 * @return #INTERRUPT_CONTINUE_EXECUTION if the operation was non-blocking, otherwise #INTERRUPT_YIELD
 */
uint32_t threadWait(struct thread *t, struct object *obj, uint32_t mode)
{
	struct linkedList *queue;
	uint32_t status = INTERRUPT_CONTINUE_EXECUTION;

	t->task.eax = 0;
	queue = __objectWait(obj, mode, &t->task.eax);
	if (queue)
	{
		assert(!t->blocked);
		t->blocked	= true;
		ll_remove(&t->obj.entry);
		ll_add_after(queue, &t->obj.entry);
		status = INTERRUPT_YIELD;
	}

	return status;
}

/**
 * @}
 */