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

#include <interrupt/interrupt.h>
#include <hardware/gdt.h>
#include <memory/physmem.h>

#include <process/object.h>
#include <process/process.h>
#include <process/thread.h>
#include <process/semaphore.h>
#include <process/pipe.h>
#include <process/event.h>
#include <process/timer.h>
#include <process/filesystem.h>

#include <console/console.h>
#include <util/util.h>
#include <syscall.h>

/** \addtogroup Interrupts
 *  @{
 *
 *  Interrupts are used for several purposes on x86 machines.
 *
 *  The first purpose is to handle software errors. When a program does
 *  something which is not allowed, like accessing an invalid memory address,
 *  or something impossible like a division by zero, an Interrupt is triggered.
 *
 *  The CPU looks up an entry in the interrupt descriptor table (IDT), which is
 *  defined by the OS, and then calls a function (or does a task switch
 *  depending on the flags defined inside the interrupt table).
 *
 *  Interrupts can also be invoked by hardware, to notify the CPU about new
 *  data like a pressed key on the keyboard. These interrupts are called IRQs
 *  and can be controlled through the \ref PIC.
 *
 *  The third way to invoke an interrupt is by using the INT opcode inside
 *  a program. This allows a programs to communicate with the kernel. In
 *  IBNOS the interrupt 0x80 is used to implement syscalls.
 *
 *  For more information about Interrupts and IRQs take a look at
 *  http://wiki.osdev.org/Interrupts
 */
static interrupt_callback interruptTable[IDT_MAX_COUNT];

 /**
  * @brief Handle an incoming interrupt
  * @details	This function handles an incoming interrupt and executes
  *				a registered interrupt handler (if any).
  *
  * @param interrupt The interrupt which should be handled
  * @param error An optional error code passed by the CPU
  * @param t The currently running thread or NULL for kernel interrupts
  * @return How to continue, take a look at the \ref InterruptReturnValue
  *			"Interrupt return values" defines
  */
uint32_t dispatchInterrupt(uint32_t interrupt, uint32_t error, struct thread *t)
{
	uint32_t status = INTERRUPT_UNHANDLED;

	/*
	consoleWriteString("interrupt(");
	consoleWriteHex32(interrupt);
	consoleWriteString(", ");
	consoleWriteHex32(error);
	consoleWriteString(", ");
	consoleWriteHex32((uint32_t)t);
	consoleWriteString(")\n");
	*/

	if (interruptTable[interrupt])
		status = interruptTable[interrupt](interrupt, error, t);

	if (t && status == INTERRUPT_UNHANDLED)
	{
		t->task.ebx = -2; /* terminate with exit code -2 */
		status = INTERRUPT_EXIT_PROCESS;
	}

	return status;
}

/**
 * @brief Coprocessor / FPU not available handler
 * @details We use hardware task switches which do not automatically
 *			save the values of the FPU. The CPU automatically protects
 *			the FPU and raises an exception if the FPU is used for the
 *			first time after a task switch. This handler saves the content of
 *			the FPU to the last task which used the FPU so that the
 *			current task can safely alter the content.
 *
 * @param interrupt Always 0x07
 * @param error Does not apply to this interrupt
 * @param t The thread which tried to use the FPU
 * @return Always #INTERRUPT_CONTINUE_EXECUTION
 */
uint32_t interrupt_0x07(UNUSED uint32_t interrupt, UNUSED uint32_t error, struct thread *t)
{
	/* we currently do not handle kernel errors */
	if (!t) return INTERRUPT_UNHANDLED;
	if (t != lastFPUthread)
	{
		asm volatile("clts");

		/* backup context of last fpu thread */
		if (lastFPUthread)
			asm volatile("fnsave %0; fwait" : "=m" (lastFPUthread->fpu));

		/* fpu was never initialized */
		if (t->fpuInitialized)
		{
			t->fpu.statusWord &= t->fpu.controlWord | 0xff80;
			asm volatile("frstor %0" : : "m" (t->fpu) );
		}
		else
		{
			asm volatile("fninit");
			t->fpuInitialized = true;
		}

		/* Remember that we restored the FPU the last time */
		lastFPUthread = t;
	}
	return INTERRUPT_CONTINUE_EXECUTION;
}

uint32_t interrupt_0x10(UNUSED uint32_t interrupt, UNUSED uint32_t error, struct thread *t)
{
	/* we currently do not handle kernel errors */
	if (!t) return INTERRUPT_UNHANDLED;
	assert(t == lastFPUthread);

	asm volatile("clts");
	asm volatile("fnsave %0; fwait" : "=m" (lastFPUthread->fpu));

	/*
	if (lastFPUthread->fpu.statusWord & 1)
		consoleWriteString("FPU invalid operation\n");
	if (lastFPUthread->fpu.statusWord & 2)
		consoleWriteString("FPU denormalized operand\n");
	if (lastFPUthread->fpu.statusWord & 4)
		consoleWriteString("FPU zero division\n");
	if (lastFPUthread->fpu.statusWord & 8)
		consoleWriteString("FPU overflow\n");
	if (lastFPUthread->fpu.statusWord & 16)
		consoleWriteString("FPU underflow\n");
	if (lastFPUthread->fpu.statusWord & 32)
		consoleWriteString("FPU precision\n");
	if (lastFPUthread->fpu.statusWord & 64)
		consoleWriteString("FPU stack fault\n");
	if (lastFPUthread->fpu.statusWord & 128)
		consoleWriteString("FPU interrupt handler\n");
	*/

	return INTERRUPT_UNHANDLED;
}

/**
 * @brief Interrupt which handles Syscalls
 * @details This function is called when a user mode program calls the
 *			interrupt 0x80. This interrupt is used to handle Syscalls
 *			and gives the user mode program the possibility to execute
 *			predefined functions in the kernel.
 *
 * @param interrupt Always 0x80
 * @param error Does not apply to this interrupt
 * @param t The thread which called the syscall
 * @return	How to continue, depends on the called function and must be one of
 *			the \ref InterruptReturnValue "Interrupt return values"
 *
 */
uint32_t interrupt_0x80(UNUSED uint32_t interrupt, UNUSED uint32_t error, struct thread *t)
{
	/* we don't expect any syscalls while running in the kernel */
	if (!t) return INTERRUPT_UNHANDLED;

	uint32_t status = INTERRUPT_CONTINUE_EXECUTION;
	uint32_t syscall = t->task.eax;
	struct userMemory k;
	struct process *p = t->process;

	/* return (-1) if the command is not found or an error occurs */
	t->task.eax = -1;

	switch (syscall)
	{
		case SYSCALL_YIELD:
			t->task.eax = 0;
			status = INTERRUPT_YIELD;
			break;

		case SYSCALL_EXIT_PROCESS:
			status = INTERRUPT_EXIT_PROCESS;
			break;

		case SYSCALL_EXIT_THREAD:
			status = INTERRUPT_EXIT_THREAD;
			break;

		case SYSCALL_GET_CURRENT_PROCESS:
			t->task.eax	= handleAllocate(&p->handles, &p->obj);
			break;

		case SYSCALL_GET_CURRENT_THREAD:
			t->task.eax	= handleAllocate(&p->handles, &t->obj);
			break;

		case SYSCALL_GET_MONOTONIC_CLOCK:
			t->task.eax = timerGetTimestamp();
			break;

		case SYSCALL_GET_PROCESS_INFO:
			if (ACCESS_USER_MEMORY_STRUCT(&k, p, (void *)t->task.ebx, t->task.ecx, sizeof(struct processInfo), true))
			{
				t->task.eax = processInfo((struct processInfo *)k.addr, t->task.ecx);
				RELEASE_USER_MEMORY(&k);
			}
			break;

		case SYSCALL_GET_THREADLOCAL_STORAGE_BASE:
			t->task.eax = (uint32_t)t->user_threadLocalBase;
			break;

		case SYSCALL_GET_THREADLOCAL_STORAGE_LENGTH:
			t->task.eax = t->user_threadLocalLength << PAGE_BITS;
			break;

		case SYSCALL_ALLOCATE_MEMORY:
			t->task.eax = (uint32_t)pagingTryAllocatePhysMem(p, t->task.ebx, true, true);
			break;

		case SYSCALL_RELEASE_MEMORY:
			t->task.eax = (uint32_t)pagingTryReleaseUserMem(p, (void *)t->task.ebx, t->task.ecx);
			break;

		case SYSCALL_FORK:
			{
				struct process *new_p = processCreate(p);
				if (new_p)
				{
					struct thread *new_t = threadCreate(new_p, t, NULL);
					if (new_t)
					{
						t->task.eax		= handleAllocate(&p->handles, &new_p->obj);
						new_t->task.eax = 0;
						objectRelease(new_t);
					}
					objectRelease(new_p);
				}
			}
			break;

		case SYSCALL_CREATE_THREAD:
			{
				struct thread *new_t = threadCreate(p, NULL, (void *)t->task.ebx);
				if (new_t)
				{
					t->task.eax	= handleAllocate(&p->handles, &new_t->obj);
					new_t->task.eax = t->task.ecx;
					new_t->task.ebx = t->task.edx;
					new_t->task.ecx = t->task.esi;
					new_t->task.edx = t->task.edi;
					objectRelease(new_t);
				}
			}
			break;

		case SYSCALL_CREATE_EVENT:
			{
				struct event *new_e = eventCreate(t->task.ebx);
				if (new_e)
				{
					t->task.eax = handleAllocate(&p->handles, &new_e->obj);
					objectRelease(new_e);
				}
			}
			break;

		case SYSCALL_CREATE_SEMAPHORE:
			{
				struct semaphore *new_s = semaphoreCreate(t->task.ebx);
				if (new_s)
				{
					t->task.eax = handleAllocate(&p->handles, &new_s->obj);
					objectRelease(new_s);
				}
			}
			break;

		case SYSCALL_CREATE_PIPE:
			{
				struct pipe *new_p = pipeCreate();
				if (new_p)
				{
					t->task.eax = handleAllocate(&p->handles, &new_p->obj);
					objectRelease(new_p);
				}
			}
			break;

		case SYSCALL_CREATE_TIMER:
			{
				struct timer *new_t = timerCreate(t->task.ebx);
				if (new_t)
				{
					t->task.eax = handleAllocate(&p->handles, &new_t->obj);
					objectRelease(new_t);
				}
			}
			break;

		case SYSCALL_OBJECT_DUP:
			{
				struct object *obj = handleGet(&p->handles, t->task.ebx);
				if (!obj) break;
				t->task.eax = handleAllocate(&p->handles, obj);
			}
			break;

		case SYSCALL_OBJECT_DUP2:
			{
				struct object *obj = handleGet(&p->handles, t->task.ebx);
				if (!obj) break;
				t->task.eax = handleSet(&p->handles, t->task.ecx, obj);
			}
			break;

		case SYSCALL_OBJECT_EXISTS:
			t->task.eax = (handleGet(&p->handles, t->task.ebx) != NULL);
			break;

		case SYSCALL_OBJECT_COMPARE:
			{
				struct object *obj1 = handleGet(&p->handles, t->task.ebx);
				struct object *obj2 = handleGet(&p->handles, t->task.ecx);
				if (obj1 || obj2) t->task.eax = (obj1 == obj2);
			}
			break;

		case SYSCALL_OBJECT_CLOSE:
			t->task.eax = handleRelease(&p->handles, t->task.ebx);
			break;

		case SYSCALL_OBJECT_SHUTDOWN:
			{
				struct object *obj = handleGet(&p->handles, t->task.ebx);
				t->task.eax = (obj != NULL);
				if (obj) __objectShutdown(obj, t->task.ecx);
				if (obj == &t->obj || obj == &p->obj) status = INTERRUPT_YIELD;
			}
			break;

		case SYSCALL_OBJECT_GET_STATUS:
			{
				struct object *obj = handleGet(&p->handles, t->task.ebx);
				if (!obj) break;
				t->task.eax = __objectGetStatus(obj, t->task.ecx);
			}
			break;

		case SYSCALL_OBJECT_WAIT:
			{
				struct object *obj = handleGet(&p->handles, t->task.ebx);
				if (!obj) break;
				status = threadWait(t, obj, t->task.ecx);
			}
			break;

		case SYSCALL_OBJECT_SIGNAL:
			{
				struct object *obj = handleGet(&p->handles, t->task.ebx);
				t->task.eax = (obj != NULL);
				if (obj) __objectSignal(obj, t->task.ecx);
			}
			break;

		case SYSCALL_OBJECT_WRITE:
			{
				struct object *obj = handleGet(&p->handles, t->task.ebx);
				if (!obj) break;
				if (ACCESS_USER_MEMORY(&k, p, (void *)t->task.ecx, t->task.edx, false))
				{
					t->task.eax = __objectWrite(obj, k.addr, t->task.edx);
					RELEASE_USER_MEMORY(&k);
				}
			}
			break;

		case SYSCALL_OBJECT_READ:
			{
				struct object *obj = handleGet(&p->handles, t->task.ebx);
				if (!obj) break;
				if (ACCESS_USER_MEMORY(&k, p, (void *)t->task.ecx, t->task.edx, true))
				{
					t->task.eax = __objectRead(obj, k.addr, t->task.edx);
					RELEASE_USER_MEMORY(&k);
				}
			}
			break;

		case SYSCALL_OBJECT_ATTACH_OBJ:
			{
				struct object *obj = handleGet(&p->handles, t->task.ebx);
				struct object *subObj = handleGet(&p->handles, t->task.ecx);
				t->task.eax = (obj && subObj) ? __objectAttachObj(obj, subObj, t->task.edx, t->task.esi) : false;
			}
			break;

		case SYSCALL_OBJECT_DETACH_OBJ:
			{
				struct object *obj = handleGet(&p->handles, t->task.ebx);
				if (!obj) break;
				t->task.eax = __objectDetachObj(obj, t->task.ecx);
			}
			break;

		case SYSCALL_CONSOLE_WRITE:
			if (ACCESS_USER_MEMORY(&k, p, (void *)t->task.ebx, t->task.ecx, true))
			{
				consoleWriteStringLen(k.addr, t->task.ecx);
				RELEASE_USER_MEMORY(&k);
				t->task.eax = t->task.ecx;
			}
			break;

		case SYSCALL_CONSOLE_WRITE_RAW:
			if (ACCESS_USER_MEMORY_STRUCT(&k, p, (void *)t->task.ebx, t->task.ecx, sizeof(uint16_t), true))
			{
				consoleWriteRawLen(k.addr, t->task.ecx);
				RELEASE_USER_MEMORY(&k);
				t->task.eax = t->task.ecx;
			}
			break;

		case SYSCALL_CONSOLE_CLEAR:
			consoleClear();
			break;

		case SYSCALL_CONSOLE_GET_SIZE:
			t->task.eax = consoleGetSize();
			break;

		case SYSCALL_CONSOLE_SET_COLOR:
			consoleSetColor(t->task.ebx);
			break;

		case SYSCALL_CONSOLE_GET_COLOR:
			t->task.eax = consoleGetColor();
			break;

		case SYSCALL_CONSOLE_SET_CURSOR:
			consoleSetCursorPos(t->task.ebx, t->task.ecx);
			break;

		case SYSCALL_CONSOLE_GET_CURSOR:
			t->task.eax = consoleGetCursorPos();
			break;

		case SYSCALL_CONSOLE_SET_HARDWARE_CURSOR:
			consoleSetHardwareCursor(t->task.ebx, t->task.ecx);
			break;

		case SYSCALL_CONSOLE_GET_HARDWARE_CURSOR:
			t->task.eax = consoleGetHardwareCursor();
			break;

		case SYSCALL_CONSOLE_SET_FLAGS:
			consoleSetFlags(t->task.ebx);
			break;

		case SYSCALL_CONSOLE_GET_FLAGS:
			t->task.eax = consoleGetFlags();
			break;

		case SYSCALL_FILESYSTEM_SEARCH_FILE:
			{
				struct directory *directory = fileSystemIsValidDirectory(handleGet(&p->handles, t->task.ebx));
				if (ACCESS_USER_MEMORY(&k, p, (void *)t->task.ecx, t->task.edx, true))
				{
					struct file *new_f = fileSystemSearchFile(directory, k.addr, t->task.edx, t->task.esi);
					if (new_f)
					{
						t->task.eax = handleAllocate(&p->handles, &new_f->obj);
						objectRelease(new_f);
					}
				}
			}
			break;

		case SYSCALL_FILESYSTEM_SEARCH_DIRECTORY:
			{
				struct directory *directory = fileSystemIsValidDirectory(handleGet(&p->handles, t->task.ebx));
				if (ACCESS_USER_MEMORY(&k, p, (void *)t->task.ecx, t->task.edx, true))
				{
					struct directory *new_d = fileSystemSearchDirectory(directory, k.addr, t->task.edx, t->task.esi);
					if (new_d)
					{
						t->task.eax = handleAllocate(&p->handles, &new_d->obj);
						objectRelease(new_d);
					}
				}
			}
			break;

		case SYSCALL_FILESYSTEM_OPEN:
			{
				struct file *f;
				struct directory *d;
				struct object *obj = handleGet(&p->handles, t->task.ebx);

				/* if the argument is a file object */
				f = fileSystemIsValidFile(obj);
				if (f)
				{
					struct openedFile *new_h = fileOpen(f);
					if (new_h)
					{
						t->task.eax = handleAllocate(&p->handles, &new_h->obj);
						objectRelease(new_h);
					}
					break;
				}

				/* if the argument is a directory object */
				d = fileSystemIsValidDirectory(obj);
				if (d)
				{
					struct openedDirectory *new_h = directoryOpen(d);
					if (new_h)
					{
						t->task.eax = handleAllocate(&p->handles, &new_h->obj);
						objectRelease(new_h);
					}
					break;
				}
			}
			break;

		default:
			status = INTERRUPT_UNHANDLED;
			break;
	}

	return status;
}

/**
 * @brief Request an interrupt
 *
 * @param interrupt The interrupt you need
 * @param callback	The function which should be called when the
 *					interrupt occurs.
 *
 * @return True if successful or false otherwise.
 */
bool interruptReserve(uint32_t interrupt, interrupt_callback callback)
{
	assert(interrupt < IDT_MAX_COUNT);
	if (interruptTable[interrupt])
		return false;

	interruptTable[interrupt] = callback;
	return true;
}

/**
 * @brief Free a previously requested interrupt
 *
 * @param interrupt The previously requested interrupt
 */
void interruptFree(uint32_t interrupt)
{
	interruptTable[interrupt] = NULL;
}

/* interrupt table */
static interrupt_callback interruptTable[IDT_MAX_COUNT] =
{
	/* 0x00 */ NULL,
	/* 0x01 */ NULL,
	/* 0x02 */ NULL,
	/* 0x03 */ NULL,
	/* 0x04 */ NULL,
	/* 0x05 */ NULL,
	/* 0x06 */ NULL,
	/* 0x07 */ interrupt_0x07,
	/* 0x08 */ NULL,
	/* 0x09 */ NULL,
	/* 0x0a */ NULL,
	/* 0x0b */ NULL,
	/* 0x0c */ NULL,
	/* 0x0d */ NULL,
	/* 0x0e */ interrupt_0x0E, /* defined in paging.c */
	/* 0x0f */ NULL,
	/* 0x10 */ interrupt_0x10,
	/* 0x11 */ NULL,
	/* 0x12 */ NULL,
	/* 0x13 */ NULL,
	/* 0x14 */ NULL,
	/* 0x15 */ NULL,
	/* 0x16 */ NULL,
	/* 0x17 */ NULL,
	/* 0x18 */ NULL,
	/* 0x19 */ NULL,
	/* 0x1a */ NULL,
	/* 0x1b */ NULL,
	/* 0x1c */ NULL,
	/* 0x1d */ NULL,
	/* 0x1e */ NULL,
	/* 0x1f */ NULL,
	/* 0x20 */ NULL,
	/* 0x21 */ NULL,
	/* 0x22 */ NULL,
	/* 0x23 */ NULL,
	/* 0x24 */ NULL,
	/* 0x25 */ NULL,
	/* 0x26 */ NULL,
	/* 0x27 */ NULL,
	/* 0x28 */ NULL,
	/* 0x29 */ NULL,
	/* 0x2a */ NULL,
	/* 0x2b */ NULL,
	/* 0x2c */ NULL,
	/* 0x2d */ NULL,
	/* 0x2e */ NULL,
	/* 0x2f */ NULL,
	/* 0x30 */ NULL,
	/* 0x31 */ NULL,
	/* 0x32 */ NULL,
	/* 0x33 */ NULL,
	/* 0x34 */ NULL,
	/* 0x35 */ NULL,
	/* 0x36 */ NULL,
	/* 0x37 */ NULL,
	/* 0x38 */ NULL,
	/* 0x39 */ NULL,
	/* 0x3a */ NULL,
	/* 0x3b */ NULL,
	/* 0x3c */ NULL,
	/* 0x3d */ NULL,
	/* 0x3e */ NULL,
	/* 0x3f */ NULL,
	/* 0x40 */ NULL,
	/* 0x41 */ NULL,
	/* 0x42 */ NULL,
	/* 0x43 */ NULL,
	/* 0x44 */ NULL,
	/* 0x45 */ NULL,
	/* 0x46 */ NULL,
	/* 0x47 */ NULL,
	/* 0x48 */ NULL,
	/* 0x49 */ NULL,
	/* 0x4a */ NULL,
	/* 0x4b */ NULL,
	/* 0x4c */ NULL,
	/* 0x4d */ NULL,
	/* 0x4e */ NULL,
	/* 0x4f */ NULL,
	/* 0x50 */ NULL,
	/* 0x51 */ NULL,
	/* 0x52 */ NULL,
	/* 0x53 */ NULL,
	/* 0x54 */ NULL,
	/* 0x55 */ NULL,
	/* 0x56 */ NULL,
	/* 0x57 */ NULL,
	/* 0x58 */ NULL,
	/* 0x59 */ NULL,
	/* 0x5a */ NULL,
	/* 0x5b */ NULL,
	/* 0x5c */ NULL,
	/* 0x5d */ NULL,
	/* 0x5e */ NULL,
	/* 0x5f */ NULL,
	/* 0x60 */ NULL,
	/* 0x61 */ NULL,
	/* 0x62 */ NULL,
	/* 0x63 */ NULL,
	/* 0x64 */ NULL,
	/* 0x65 */ NULL,
	/* 0x66 */ NULL,
	/* 0x67 */ NULL,
	/* 0x68 */ NULL,
	/* 0x69 */ NULL,
	/* 0x6a */ NULL,
	/* 0x6b */ NULL,
	/* 0x6c */ NULL,
	/* 0x6d */ NULL,
	/* 0x6e */ NULL,
	/* 0x6f */ NULL,
	/* 0x70 */ NULL,
	/* 0x71 */ NULL,
	/* 0x72 */ NULL,
	/* 0x73 */ NULL,
	/* 0x74 */ NULL,
	/* 0x75 */ NULL,
	/* 0x76 */ NULL,
	/* 0x77 */ NULL,
	/* 0x78 */ NULL,
	/* 0x79 */ NULL,
	/* 0x7a */ NULL,
	/* 0x7b */ NULL,
	/* 0x7c */ NULL,
	/* 0x7d */ NULL,
	/* 0x7e */ NULL,
	/* 0x7f */ NULL,
	/* 0x80 */ interrupt_0x80,
	/* 0x81 */ NULL,
	/* 0x82 */ NULL,
	/* 0x83 */ NULL,
	/* 0x84 */ NULL,
	/* 0x85 */ NULL,
	/* 0x86 */ NULL,
	/* 0x87 */ NULL,
	/* 0x88 */ NULL,
	/* 0x89 */ NULL,
	/* 0x8a */ NULL,
	/* 0x8b */ NULL,
	/* 0x8c */ NULL,
	/* 0x8d */ NULL,
	/* 0x8e */ NULL,
	/* 0x8f */ NULL,
	/* 0x90 */ NULL,
	/* 0x91 */ NULL,
	/* 0x92 */ NULL,
	/* 0x93 */ NULL,
	/* 0x94 */ NULL,
	/* 0x95 */ NULL,
	/* 0x96 */ NULL,
	/* 0x97 */ NULL,
	/* 0x98 */ NULL,
	/* 0x99 */ NULL,
	/* 0x9a */ NULL,
	/* 0x9b */ NULL,
	/* 0x9c */ NULL,
	/* 0x9d */ NULL,
	/* 0x9e */ NULL,
	/* 0x9f */ NULL,
	/* 0xa0 */ NULL,
	/* 0xa1 */ NULL,
	/* 0xa2 */ NULL,
	/* 0xa3 */ NULL,
	/* 0xa4 */ NULL,
	/* 0xa5 */ NULL,
	/* 0xa6 */ NULL,
	/* 0xa7 */ NULL,
	/* 0xa8 */ NULL,
	/* 0xa9 */ NULL,
	/* 0xaa */ NULL,
	/* 0xab */ NULL,
	/* 0xac */ NULL,
	/* 0xad */ NULL,
	/* 0xae */ NULL,
	/* 0xaf */ NULL,
	/* 0xb0 */ NULL,
	/* 0xb1 */ NULL,
	/* 0xb2 */ NULL,
	/* 0xb3 */ NULL,
	/* 0xb4 */ NULL,
	/* 0xb5 */ NULL,
	/* 0xb6 */ NULL,
	/* 0xb7 */ NULL,
	/* 0xb8 */ NULL,
	/* 0xb9 */ NULL,
	/* 0xba */ NULL,
	/* 0xbb */ NULL,
	/* 0xbc */ NULL,
	/* 0xbd */ NULL,
	/* 0xbe */ NULL,
	/* 0xbf */ NULL,
	/* 0xc0 */ NULL,
	/* 0xc1 */ NULL,
	/* 0xc2 */ NULL,
	/* 0xc3 */ NULL,
	/* 0xc4 */ NULL,
	/* 0xc5 */ NULL,
	/* 0xc6 */ NULL,
	/* 0xc7 */ NULL,
	/* 0xc8 */ NULL,
	/* 0xc9 */ NULL,
	/* 0xca */ NULL,
	/* 0xcb */ NULL,
	/* 0xcc */ NULL,
	/* 0xcd */ NULL,
	/* 0xce */ NULL,
	/* 0xcf */ NULL,
	/* 0xd0 */ NULL,
	/* 0xd1 */ NULL,
	/* 0xd2 */ NULL,
	/* 0xd3 */ NULL,
	/* 0xd4 */ NULL,
	/* 0xd5 */ NULL,
	/* 0xd6 */ NULL,
	/* 0xd7 */ NULL,
	/* 0xd8 */ NULL,
	/* 0xd9 */ NULL,
	/* 0xda */ NULL,
	/* 0xdb */ NULL,
	/* 0xdc */ NULL,
	/* 0xdd */ NULL,
	/* 0xde */ NULL,
	/* 0xdf */ NULL,
	/* 0xe0 */ NULL,
	/* 0xe1 */ NULL,
	/* 0xe2 */ NULL,
	/* 0xe3 */ NULL,
	/* 0xe4 */ NULL,
	/* 0xe5 */ NULL,
	/* 0xe6 */ NULL,
	/* 0xe7 */ NULL,
	/* 0xe8 */ NULL,
	/* 0xe9 */ NULL,
	/* 0xea */ NULL,
	/* 0xeb */ NULL,
	/* 0xec */ NULL,
	/* 0xed */ NULL,
	/* 0xee */ NULL,
	/* 0xef */ NULL,
	/* 0xf0 */ NULL,
	/* 0xf1 */ NULL,
	/* 0xf2 */ NULL,
	/* 0xf3 */ NULL,
	/* 0xf4 */ NULL,
	/* 0xf5 */ NULL,
	/* 0xf6 */ NULL,
	/* 0xf7 */ NULL,
	/* 0xf8 */ NULL,
	/* 0xf9 */ NULL,
	/* 0xfa */ NULL,
	/* 0xfb */ NULL,
	/* 0xfc */ NULL,
	/* 0xfd */ NULL,
	/* 0xfe */ NULL,
	/* 0xff */ NULL
};

/** @}*/
