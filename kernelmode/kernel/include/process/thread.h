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

#ifndef _H_THREAD_
#define _H_THREAD_


#ifdef __KERNEL__

	struct thread;

	#include <stdint.h>
	#include <stdbool.h>

	#include <process/object.h>
	#include <process/process.h>
	#include <hardware/context.h>
	#include <util/list.h>

	extern struct linkedList threadList;
	extern struct thread *lastFPUthread;

	struct thread
	{
		struct object obj;
		struct linkedList waiters;
		bool blocked;

		/* entry in the list of the associated process (not refcounted) */
		struct process *process;
		struct linkedList entry_process;
		uint32_t exitcode;

		/* fpu was initialized for this thread */
		bool fpuInitialized;

		/* ring3 stack mapped in user space  */
		void *user_ring3StackBase;
		uint32_t user_ring3StackLength; /* in pages */

		/* thread local block in user space */
		void *user_threadLocalBase;
		uint32_t user_threadLocalLength; /* in pages */

		struct taskContext task;
		struct fpuContext fpu;
	};

	#define DEFAULT_STACK_SIZE	0x10000
	#define DEFAULT_TLB_SIZE	0x1000

	struct thread *threadCreate(struct process *p, struct thread *original, void *eip);
	struct thread *threadRun(struct thread *t);
	void threadRelease(struct thread *t);

	void threadSchedule();

	uint32_t threadWait(struct thread *t, struct object *obj, uint32_t mode);

#endif

#endif /* _H_THREAD_ */