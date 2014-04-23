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

#include <hardware/gdt.h>
#include <memory/paging.h>
#include <memory/physmem.h>
#include <console/console.h>
#include <interrupt/interrupt.h>
#include <process/thread.h>

/**
 * \defgroup GDT Global / Interrupt Descriptor Table
 * \addtogroup GDT
 * @{
 */

/* kernelstack */
void *kernelStack;

/* gdt */
static struct GDTTable gdtTable;
static struct GDTEntry *gdtTableEntries;

/* idt */
static struct IDTTable idtTable;
static struct IDTEntry *idtTableEntries;

/* intjmp */
void *intJmpTable_kernel;
void *intJmpTable_user;

/* task */
static struct taskContext *taskTable;

static struct taskContext *TSS_kernel;
static struct taskContext *TSS_user;

/* code and data segments */
struct GDTEntry *codeRing0;
struct GDTEntry *dataRing0;
struct GDTEntry *codeRing3;
struct GDTEntry *dataRing3;

/* task segments */
struct GDTEntry *kernelTask;
struct GDTEntry *usermodeTask;

#define INTJMP_ENTRY_SIZE 8
#define INTJMP_ENTRY_MASK 7
#define INTJMP_ENTRY_BITS 3

/* used to determine the memory locations of the idle part */
extern uint32_t __kernelIdleBegin;
extern uint32_t __kernelIdleEnd;
#define KERNEL_IDLE_BEGIN	((uint32_t)&__kernelIdleBegin)
#define KERNEL_IDLE_END		((uint32_t)&__kernelIdleEnd)

static const char *error_outOfGDTEntries[] =
{
	" GDT ERROR ",
	" No more GDT entries left!",
	NULL
};

static const char *error_unhandledKernelInterrupt[] =
{
	" KERNEL INTERRUPT ",
	" Unable to handle kernel interrupt! ",
	NULL
};

static const char *error_usermodeInterruptInvalid[] =
{
	" USERMODE INTERRUPT ",
	" Unable to recover from usermode interrupt! ",
	NULL
};

void __attribute__((cdecl)) __setGDT(const struct GDTTable *table);
asm(".text\n.align 4\n"
"__setGDT:\n"
"	movl 4(%esp), %eax\n"
"	lgdt (%eax)\n"
"	ret\n"
);

void __attribute__((cdecl)) __setSegments(uint32_t code, uint32_t data);
asm(".text\n.align 4\n"
"__setSegments:\n"
"	movw 8(%esp), %ax\n"
"	movw %ax, %ds\n"
"	movw %ax, %es\n"
"	movw %ax, %fs\n"
"	movw %ax, %gs\n"
"	movw %ax, %ss\n"
"	popl %eax\n"
"	pushl (%esp)\n"
"	pushl %eax\n"
"	retf\n"
);

void __attribute__((cdecl)) __loadTSS(uint16_t value);
asm(".text\n.align 4\n"
"__loadTSS:\n"
"	movw 4(%esp), %ax\n"
"	ltr %ax\n"
"	ret\n"
);

uint32_t __attribute__((cdecl)) __runUserModeTask(uint16_t task);
asm(".text\n.align 4\n"
"__runUserModeTask:\n"
"	pushw 4(%esp)\n"
"	pushl $0\n"
"	ljmp *(%esp)\n"
"	add $6, %esp\n"
"	ret\n"
);

void __attribute__((cdecl)) __setIDT(const struct IDTTable *table);
asm(".text\n.align 4\n"
"__setIDT:\n"
"	movl 4(%esp), %eax\n"
"	lidt (%eax)\n"
"	ret\n"
);

#define __isErrorCodeInterrupt(i) \
	((i) == 8 || ((i) >= 10 && (i) <= 14) || (i) == 17)

/**
 * @brief Dispatcher routine for interrupts occuring in kernel code
 *
 * @param interrupt The interrupt which should be handled
 * @param error An optional error code passed by the CPU
 * @param context Always NULL (because the interrupt was in kernel code)
 */
static __attribute__((cdecl)) void __dispatchKernelInterrupt(uint32_t interrupt, uint32_t error, struct taskContext *context)
{
	uint32_t status = dispatchInterrupt(interrupt, error, NULL);

	/* unable to process kernel interrupt - show bluescreen */
	if (status != INTERRUPT_CONTINUE_EXECUTION && status != INTERRUPT_YIELD)
	{
		uint32_t cr2;
		asm volatile("mov %%cr2, %0" : "=r" (cr2));
		uint32_t args[] = {interrupt, error, status, cr2};
		consoleSystemFailure(error_unhandledKernelInterrupt, sizeof(args)/sizeof(args[0]), args, context);
	}

	/* kernel was idling */
	if (context->eip >= KERNEL_IDLE_BEGIN && context->eip < KERNEL_IDLE_END)
	{
		context->eflags &= ~(1 << 9); /* disable interrupts again */
		context->eip	 = KERNEL_IDLE_END;
	}
}

/**
 * @brief Initializes the Global Descriptor Table
 * @details This function initializes the global descriptor table, and adds all
 *			the required selectors. For now we only need 4 segments: Two for the
 *			kernel (code ring0, data ring0) and two for the usermode (code ring3,
 *			data ring3).
 */
static void __initBasicGDT()
{
	codeRing0 = gdtGetFreeEntry();
	gdtEntrySetAddress(codeRing0, 0);
	gdtEntrySetLimit(codeRing0, 0x100000000);
	codeRing0->accessBits.accessed  = 0;
	codeRing0->accessBits.readWrite = 1;
	codeRing0->accessBits.dc		= 0;
	codeRing0->accessBits.execute   = 1;
	codeRing0->accessBits.isSystem  = 1;
	codeRing0->accessBits.privlevel = GDT_CPL_RING0;
	codeRing0->accessBits.present   = 1;
	codeRing0->user     = 0;
	codeRing0->reserved = 0;
	codeRing0->is32bit  = 1;

	dataRing0 = gdtGetFreeEntry();
	gdtEntrySetAddress(dataRing0, 0);
	gdtEntrySetLimit(dataRing0, 0x100000000);
	dataRing0->accessBits.accessed	= 0;
	dataRing0->accessBits.readWrite	= 1;
	dataRing0->accessBits.dc		= 0;
	dataRing0->accessBits.execute	= 0;
	dataRing0->accessBits.isSystem	= 1;
	dataRing0->accessBits.privlevel	= GDT_CPL_RING0;
	dataRing0->accessBits.present	= 1;
	dataRing0->user     = 0;
	dataRing0->reserved = 0;
	dataRing0->is32bit  = 1;

	codeRing3 = gdtGetFreeEntry();
	gdtEntrySetAddress(codeRing3, 0);
	gdtEntrySetLimit(codeRing3, 0x100000000);
	codeRing3->accessBits.accessed  = 0;
	codeRing3->accessBits.readWrite = 1;
	codeRing3->accessBits.dc		= 0;
	codeRing3->accessBits.execute   = 1;
	codeRing3->accessBits.isSystem  = 1;
	codeRing3->accessBits.privlevel = GDT_CPL_RING3;
	codeRing3->accessBits.present   = 1;
	codeRing3->user     = 0;
	codeRing3->reserved = 0;
	codeRing3->is32bit  = 1;

	dataRing3 = gdtGetFreeEntry();
	gdtEntrySetAddress(dataRing3, 0);
	gdtEntrySetLimit(dataRing3, 0x100000000);
	dataRing3->accessBits.accessed	= 0;
	dataRing3->accessBits.readWrite	= 1;
	dataRing3->accessBits.dc		= 0;
	dataRing3->accessBits.execute	= 0;
	dataRing3->accessBits.isSystem	= 1;
	dataRing3->accessBits.privlevel	= GDT_CPL_RING3;
	dataRing3->accessBits.present	= 1;
	dataRing3->user     = 0;
	dataRing3->reserved = 0;
	dataRing3->is32bit  = 1;
}

/**
 * @brief Initializes the task registers
 * @details This function initializes the task registers. Since this is just a
 *			single processor operating system two tasks are sufficient: one for
 *			the kernel, and one for the current usermode process.
 */
static void __initBasicTask()
{
	struct taskContext *task = taskTable;
	assert(taskTable);

	kernelTask = gdtGetFreeEntry();
	gdtEntrySetAddress(kernelTask, (uint32_t)task);
	gdtEntrySetLimit(kernelTask, sizeof(*task));
	kernelTask->accessBits.accessed  = 1;
	kernelTask->accessBits.readWrite = 0; /* busy flag for tasks */
	kernelTask->accessBits.dc		 = 0;
	kernelTask->accessBits.execute   = 1;
	kernelTask->accessBits.isSystem  = 0;
	kernelTask->accessBits.privlevel = GDT_CPL_RING0;
	kernelTask->accessBits.present   = 1;
	kernelTask->user     = 0;
	kernelTask->reserved = 0;
	kernelTask->is32bit  = 0; /* not used for tasks */

	TSS_kernel = task;
	memset(task, 0, sizeof(*task));
	task->ldt		= 0;
	task->iomap		= sizeof(*task);
	task++;

	usermodeTask = gdtGetFreeEntry();
	gdtEntrySetAddress(usermodeTask, (uint32_t)task);
	gdtEntrySetLimit(usermodeTask, sizeof(*task));
	usermodeTask->accessBits.accessed  = 1;
	usermodeTask->accessBits.readWrite = 0; /* busy flag for tasks */
	usermodeTask->accessBits.dc		   = 0;
	usermodeTask->accessBits.execute   = 1;
	usermodeTask->accessBits.isSystem  = 0;
	usermodeTask->accessBits.privlevel = GDT_CPL_RING0;
	usermodeTask->accessBits.present   = 1;
	usermodeTask->user     = 0;
	usermodeTask->reserved = 0;
	usermodeTask->is32bit  = 0; /* not used for tasks */

	TSS_user = task;
	memset(task, 0, sizeof(*task));
	task->ldt		= 0;
	task->iomap		= sizeof(*task);
	task++;

	/* ensure that all pointers are within the single task page */
	assert((uint32_t)task - (uint32_t)taskTable <= PAGE_SIZE);
}

/**
 * @brief Generates the interrupt handler jump tables
 * @details This function fills out the memory block for the interrupt handler jump
 *			tables with the appropriate opcodes. The main purpose is to transfer
 *			control from the usermode application or the location in the kernel code
 *			to the interrupt handler function, which will then try to handle the issue.
 */
static void __generateIntJmpTables()
{
	uint32_t i;
	uint8_t *cur, *dispatcher;

	assert(intJmpTable_kernel && intJmpTable_user);
	assert(INTJMP_ENTRY_SIZE * IDT_MAX_COUNT <= PAGE_SIZE);
	assert(kernelTask);

	/*
	 * Kernel mode interrupt table
	 */

	dispatcher = (uint8_t *)intJmpTable_kernel + INTJMP_ENTRY_SIZE * IDT_MAX_COUNT;

	for (i = 0; i < IDT_MAX_COUNT; i++)
	{
		cur = (uint8_t *)intJmpTable_kernel + INTJMP_ENTRY_SIZE * i;

		if (!__isErrorCodeInterrupt(i))
		{
			*cur++ = 0x83; *cur++ = 0xEC; *cur++ = 0x6C;		/* sub esp, 0x6C */
		}
		else
		{
			*cur++ = 0x83; *cur++ = 0xEC; *cur++ = 0x68;		/* sub esp, 0x68 */
		}

		*cur++ = 0xE8;											/* call <dispatcher> */
		*(uint32_t *)cur = dispatcher - (cur + 4); cur += 4;
	}

	/* STACK LAYOUT:
	 *
	 *  esp + 0x00: local call (used to determine interrupt number)
	 *  esp + 0x04: context (length: 0x68)
	 *  esp + 0x6C: errorcode or undefined
	 *  esp + 0x70: eip
	 *  esp + 0x74: cs
	 *  esp + 0x78: eflags
	 *
	 */

	cur = dispatcher;											/* <dispatcher>: */
	*cur++ = 0x89; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x2C; /* mov DWORD PTR [esp+0x2C],eax */
	*cur++ = 0x89; *cur++ = 0x4C; *cur++ = 0x24; *cur++ = 0x30; /* mov DWORD PTR [esp+0x30],ecx */
	*cur++ = 0x89; *cur++ = 0x54; *cur++ = 0x24; *cur++ = 0x34; /* mov DWORD PTR [esp+0x34],edx */
	*cur++ = 0x89; *cur++ = 0x5C; *cur++ = 0x24; *cur++ = 0x38; /* mov DWORD PTR [esp+0x38],ebx */
	*cur++ = 0x8D; *cur++ = 0x84; *cur++ = 0x24; *cur++ = 0x80; /* lea eax,[esp+0x80] */
	*cur++ = 0x00; *cur++ = 0x00; *cur++ = 0x00;
	*cur++ = 0x89; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x3C; /* mov DWORD PTR [esp+0x3C],eax */
	*cur++ = 0x89; *cur++ = 0x6C; *cur++ = 0x24; *cur++ = 0x40; /* mov DWORD PTR [esp+0x40],ebp */
	*cur++ = 0x89; *cur++ = 0x74; *cur++ = 0x24; *cur++ = 0x44; /* mov DWORD PTR [esp+0x44],esi */
	*cur++ = 0x89; *cur++ = 0x7C; *cur++ = 0x24; *cur++ = 0x48; /* mov DWORD PTR [esp+0x48],edi */
	*cur++ = 0x8C; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x4C; /* mov WORD PTR [esp+0x4C],es */
	*cur++ = 0x66; *cur++ = 0x8B; *cur++ = 0x44; *cur++ = 0x24; /* mov ax,WORD PTR [esp+0x74] */
	*cur++ = 0x74;
	*cur++ = 0x66; *cur++ = 0x89; *cur++ = 0x44; *cur++ = 0x24; /* mov WORD PTR [esp+0x50],ax */
	*cur++ = 0x50;
	*cur++ = 0x8C; *cur++ = 0x54; *cur++ = 0x24; *cur++ = 0x54; /* mov WORD PTR [esp+0x54],ss */
	*cur++ = 0x8C; *cur++ = 0x5C; *cur++ = 0x24; *cur++ = 0x58; /* mov WORD PTR [esp+0x58],ds */
	*cur++ = 0x8C; *cur++ = 0x64; *cur++ = 0x24; *cur++ = 0x5C; /* mov WORD PTR [esp+0x5C],fs */
	*cur++ = 0x8C; *cur++ = 0x6C; *cur++ = 0x24; *cur++ = 0x60; /* mov WORD PTR [esp+0x60],gs */
	*cur++ = 0x0F; *cur++ = 0x20; *cur++ = 0xD8;				/* mov eax,cr3 */
	*cur++ = 0x89; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x20; /* mov DWORD PTR [esp+0x20],eax */
	*cur++ = 0x8B; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x70; /* mov eax,DWORD PTR [esp+0x70] */
	*cur++ = 0x89; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x24; /* mov DWORD PTR [esp+0x24],eax */
	*cur++ = 0x8B; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x78; /* mov eax,DWORD PTR [esp+0x78] */
	*cur++ = 0x89; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x28; /* mov DWORD PTR [esp+0x28],eax */
	*cur++ = 0x58;												/* pop eax (interrupt addr) */
	*cur++ = 0x2D;												/* sub eax, <offset> */
	*(uint32_t *)cur = (USERMODE_INTJMP_ADDRESS + 8); cur += 4;
	*cur++ = 0xC1; *cur++ = 0xE8; *cur++ = 0x03;				/* shr eax, 0x3 */
	*cur++ = 0x54;												/* push esp (context) */
	*cur++ = 0xFF; *cur++ = 0x74; *cur++ = 0x24; *cur++ = 0x6C; /* push DWORD PTR [esp+0x6c] (error code) */
	*cur++ = 0x50;												/* push eax (interrupt nr) */
	*cur++ = 0xE8;												/* call <address> */
	*(uint32_t *)cur = (uint8_t *)&__dispatchKernelInterrupt - (cur + 4); cur += 4;
	*cur++ = 0x8B; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x2C; /* mov eax,DWORD PTR [esp+0x2C] */
	*cur++ = 0x89; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x78; /* mov DWORD PTR [esp+0x78],eax */
	*cur++ = 0x8B; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x58; /* mov eax,DWORD PTR [esp+0x58] */
	*cur++ = 0x66; *cur++ = 0x89; *cur++ = 0x44; *cur++ = 0x24; /* mov WORD PTR [esp+0x7C],ax */
	*cur++ = 0x7C;
	*cur++ = 0x8B; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x30; /* mov eax,DWORD PTR [esp+0x30] */
	*cur++ = 0x89; *cur++ = 0x84; *cur++ = 0x24; *cur++ = 0x80; /* mov DWORD PTR [esp+0x80],eax */
	*cur++ = 0x00; *cur++ = 0x00; *cur++ = 0x00;
	*cur++ = 0x8B; *cur++ = 0x7C; *cur++ = 0x24; *cur++ = 0x50; /* mov edi,DWORD PTR [esp+0x50] */
	*cur++ = 0x8B; *cur++ = 0x74; *cur++ = 0x24; *cur++ = 0x4C; /* mov esi,DWORD PTR [esp+0x4C] */
	*cur++ = 0x8B; *cur++ = 0x6C; *cur++ = 0x24; *cur++ = 0x48; /* mov ebp,DWORD PTR [esp+0x48] */
	*cur++ = 0x8B; *cur++ = 0x5C; *cur++ = 0x24; *cur++ = 0x40; /* mov ebx,DWORD PTR [esp+0x40] */
	*cur++ = 0x8B; *cur++ = 0x54; *cur++ = 0x24; *cur++ = 0x3C; /* mov edx,DWORD PTR [esp+0x3C] */
	*cur++ = 0x8B; *cur++ = 0x4C; *cur++ = 0x24; *cur++ = 0x38; /* mov ecx,DWORD PTR [esp+0x38] */
	*cur++ = 0x8B; *cur++ = 0x44; *cur++ = 0x24; *cur++ = 0x34; /* mov eax,DWORD PTR [esp+0x34] */
	*cur++ = 0x83; *cur++ = 0xC4; *cur++ = 0x78;				/* add esp, 0x78 */
	*cur++ = 0xCF;												/* iret */
	assert(cur <= (uint8_t *)intJmpTable_kernel + PAGE_SIZE);

	/*
	 * Usermode interrupt table
	 */

	for (i = 0; i < IDT_MAX_COUNT; i++)
	{
		cur = (uint8_t *)intJmpTable_user + INTJMP_ENTRY_SIZE * i;

		*cur++ = 0xEA; *(uint32_t *)cur = 0; cur += 4;			/* jmp seg:addr */
		*(uint16_t *)cur = gdtGetEntryOffset(kernelTask, GDT_CPL_RING0); cur += 2;
		*cur++ = 0xCC;											/* int 3 */
	}

	dispatcher = (uint8_t *)intJmpTable_user + (USERMODE_INTJMP_ENABLE_FPU - USERMODE_INTJMP_ADDRESS);

	cur = dispatcher;											/* <enable_fpu>: */
	*cur++ = 0x0F; *cur++ = 0x06;								/* clts */
	*cur++ = 0xCF;												/* iret */
	assert(cur <= (uint8_t *)intJmpTable_user + PAGE_SIZE);
}

/**
 * @brief Initializes the GDT, task registers, and sets up everything required for multiprocessing.
 */
void gdtInit()
{
	uint32_t i;

	assert(!gdtTableEntries);
	assert(!idtTableEntries);
	assert(!taskTable);
	assert(!intJmpTable_kernel && !intJmpTable_user);

	assert(GDT_MAX_COUNT * sizeof(struct GDTEntry) == GDT_MAX_SIZE);
	assert(GDT_MAX_PAGES * PAGE_SIZE == GDT_MAX_SIZE);
	assert(IDT_MAX_COUNT * sizeof(struct IDTEntry) <= PAGE_SIZE);
	assert(2 * sizeof(struct taskContext) <= PAGE_SIZE);

	kernelStack	= pagingAllocatePhysMemUnpageable(NULL, 1, true, false);
	memset(kernelStack, 0, PAGE_SIZE);

	gdtTableEntries = (struct GDTEntry *)pagingAllocatePhysMemFixedUnpageable(NULL, (void *)USERMODE_GDT_ADDRESS, GDT_MAX_PAGES, true, false);
	memset(gdtTableEntries, 0, sizeof(struct GDTEntry) * GDT_MAX_COUNT);

	idtTableEntries = (struct IDTEntry *)pagingAllocatePhysMemFixedUnpageable(NULL, (void *)USERMODE_IDT_ADDRESS, 1, true, false);
	memset(idtTableEntries, 0, PAGE_SIZE);

	taskTable = (struct taskContext *)pagingAllocatePhysMemFixedUnpageable(NULL, (void *)USERMODE_TASK_ADDRESS, 1, true, false);
	memset(taskTable, 0, PAGE_SIZE);

	intJmpTable_kernel = pagingAllocatePhysMemFixedUnpageable(NULL, (void *)USERMODE_INTJMP_ADDRESS, 1, true, false);
	memset(intJmpTable_kernel, 0, PAGE_SIZE);

	intJmpTable_user = pagingAllocatePhysMemUnpageable(NULL, 1, true, false);
	memset(intJmpTable_user, 0, PAGE_SIZE);

	/* gdt */
	__initBasicGDT();
	gdtTable.limit   = GDT_MAX_SIZE - 1;
	gdtTable.address = (uint32_t)gdtTableEntries;
	__setGDT(&gdtTable);
	__setSegments(gdtGetEntryOffset(codeRing0, GDT_CPL_RING0), gdtGetEntryOffset(dataRing0, GDT_CPL_RING0));

	/* task */
	__initBasicTask();
	debugCaptureCpuContext(TSS_kernel);
	__loadTSS(gdtGetEntryOffset(kernelTask, GDT_CPL_RING0));

	/* idt */
	__generateIntJmpTables();

	for (i = 0; i < IDT_MAX_COUNT; i++)
	{
		struct IDTEntry *entry = &idtTableEntries[i];
		uint32_t address = USERMODE_INTJMP_ADDRESS + INTJMP_ENTRY_SIZE * i;

		entry->addressLow	= address & 0x0000FFFF;
		entry->csSelector	= gdtGetEntryOffset(codeRing0, GDT_CPL_RING0);
		entry->zero						= 0;
		entry->typeBits.type			= INT_TYPE_INT32;
		entry->typeBits.storageSegment	= 0;
		entry->typeBits.dpl				= (i == 0x80) ? GDT_CPL_RING3 : GDT_CPL_RING0; /* FIXME: don't hardcode the syscall interrupt */
		entry->typeBits.present			= 1;
		entry->addressHigh	= address >> 16;
	}

	idtTable.limit   = IDT_MAX_COUNT * sizeof(struct IDTEntry) - 1;
	idtTable.address = (uint32_t)idtTableEntries;
	__setIDT(&idtTable);
}

/**
 * @brief Get a free entry in the GDT
 * @return Pointer to the free GDT entry
 */
struct GDTEntry* gdtGetFreeEntry()
{
	uint32_t i;

	assert(gdtTableEntries);

	/* we skip the first entry, the so called null descriptor */
	for (i = 1; i < GDT_MAX_COUNT; i++)
	{
		if (!gdtTableEntries[i].accessBits.present)
		{
			gdtTableEntries[i].accessBits.present = 1;
			return &gdtTableEntries[i];
		}
	}

	SYSTEM_FAILURE(error_outOfGDTEntries);
}

/**
 * @brief Determines the offset of a GDT entry
 * @details This functions calculates the offset of an entry relative to the base
 *			address of the GDT. This CPU always uses offsets to identify an entry,
 *			so that the absolute address of an entry is only useful for changing it.
 *			These offsets are for example required to specify a segment as value in
 *			a segment register (CS, DS, SS, ...) or an task inside the task register.
 *			The CPU also stores the needed privilege level (ring) inside such a register.
 *			To simplify your work, this function will embed the ring passed as parameter.
 *
 * @param[in] entry The entry for which you want to get the offset
 * @param[in] ring The ring which is required to access the segment
 *
 * @return Offset of the entry + the ring level
 */
uint32_t gdtGetEntryOffset(struct GDTEntry* entry, uint32_t ring)
{
	uint32_t offset;
	assert(gdtTableEntries && entry >= gdtTableEntries && entry < gdtTableEntries + GDT_MAX_COUNT);

	offset = (uint32_t)entry - (uint32_t)gdtTableEntries;
	assert((offset & 7) == 0);

	return offset | ring;
}

/**
 * @brief Helper function to set the address inside a GDTEntry
 * @details The address value is split across 3 attributes inside the GDTEntry
 *			structure (GDTEntry::address1, GDTEntry::address2, GDTEntry::address3).
 *			This functions should make it easier to set this value.
 *
 * @param[in] entry The entry which should be altered
 * @param[in] address The address which should be set
 */
void gdtEntrySetAddress(struct GDTEntry* entry, uint32_t address)
{
	assert(entry);

	entry->address1 =  address & 0x0000FFFF;
	entry->address2 = (address & 0x00FF0000) >> 16;
	entry->address3 = (address & 0xFF000000) >> 24;
}

/**
 * @brief Helper function to set the length inside a GDTEntry
 * @details The length value or to be more precise the limit (length - 1) is split
 *			across 2 attributes inside the GDTEntry structure (GDTEntry::limit1,
 *			GDTEntry::limit2). All these fields can only save
 *			a value with a maximum of 20 bits. To store a length with a higher
 *			value (for example for a segment covering the whole address space),
 *			the value can be stored as pages. The function will automatically set
 *			the GDTEntry::granularity bit to identify the limit as page count.
 *
 * @warning If the length exceeds 1 megabyte, it must be a multiple of
 *			of the page size or an assertion will be hit.
 *
 * @param[in] entry The entry which should be altered
 * @param[in] length The length which should be set
 */
void gdtEntrySetLimit(struct GDTEntry* entry, uint64_t length)
{
	assert(entry);
	assert(length <= 0x100000000);
	assert(length > 0);

	if (length > 0x100000)
	{
		assert((length & PAGE_MASK) == 0);
		length >>= PAGE_BITS;
		entry->granularity = 1; /* 4 KB blocks */
	}
	else
	{
		entry->granularity = 0; /* 1 byte blocks */
	}

	length--;

	entry->limit1 = length & 0x0000FFFF;
	entry->limit2 = (length >> 16) & 0xF;
}

/**
 * @brief Mark a GDTEntry as free
 * @details This function is used to mark a GDT entry to be used for other purposes.
 *			You are not allowed to access the entry any more after releasing it.
 *
 * @param[in] entry The entry to release
 */
void gdtReleaseEntry(struct GDTEntry* entry)
{
	assert(entry);

	/* reset all flags including present */
	memset(entry, 0, sizeof(struct GDTEntry));
}

/**
 * @brief Run a thread
 * @details This function loads the saved thread context inside a hardware task and starts it.
 *
 * @param[in] t A pointer to a thread structure containing the tss context
 *
 * @return Reason why the execution was interrupted, usually one of the \ref InterruptReturnValue "Interrupt return values"
 */
uint32_t tssRunUsermodeThread(struct thread *t)
{
	uint32_t interrupt, error = 0;
	uint32_t *args;

	assert(t);

	/* initialize user context with provided context */
	*TSS_user = t->task;

	/* if FPU is enabled, we have to return in ring0, then clear the TS bit,
	 * and afterwards switch back to the original ring3 code. */
	if (t == lastFPUthread)
	{
		args = (uint32_t *)((uint32_t)kernelStack + PAGE_SIZE - 5 * sizeof(uint32_t));
		args[0] = TSS_user->eip;
		args[1] = TSS_user->cs;
		args[2] = TSS_user->eflags;
		args[3] = TSS_user->esp;
		args[4] = TSS_user->ss;

		TSS_user->eip		= USERMODE_INTJMP_ENABLE_FPU;
		TSS_user->cs		= gdtGetEntryOffset(codeRing0, GDT_CPL_RING0);
		TSS_user->eflags	= 0; /* real eflags will be restored later, it is important that interrupts are still disabled */
		TSS_user->esp		= USERMODE_KERNELSTACK_LIMIT - 5 * sizeof(uint32_t);
		TSS_user->ss		= gdtGetEntryOffset(dataRing0, GDT_CPL_RING0);
	}

	__runUserModeTask(gdtGetEntryOffset(usermodeTask, GDT_CPL_RING0));
	t->task = *TSS_user;

	assert(TSS_user->ss == t->task.ss0);

	/* determine which interrupt it was */
	interrupt = (t->task.eip - (USERMODE_INTJMP_ADDRESS + 7));
	assert((interrupt & INTJMP_ENTRY_MASK) == 0);
	interrupt >>= INTJMP_ENTRY_BITS;
	assert((interrupt & ~255) == 0);

	/* read args from kernel stack in the user process */
	args = (uint32_t *)((uint32_t)kernelStack + TSS_user->esp - USERMODE_KERNELSTACK_ADDRESS);
	if (TSS_user->esp == USERMODE_KERNELSTACK_LIMIT - 6 * sizeof(uint32_t) /* && __isErrorCodeInterrupt(interrupt) */)
	{
		error			= args[0];
		t->task.eip		= args[1];
		t->task.cs		= args[2];
		t->task.eflags	= args[3];
		t->task.esp		= args[4];
		t->task.ss		= args[5];
	}
	else if (TSS_user->esp == USERMODE_KERNELSTACK_LIMIT - 5 * sizeof(uint32_t))
	{
		t->task.eip		= args[0];
		t->task.cs		= args[1];
		t->task.eflags	= args[2];
		t->task.esp		= args[3];
		t->task.ss		= args[4];
	}
	else
		consoleSystemFailure(error_usermodeInterruptInvalid, 0, NULL, &t->task);

	/* we should now be again in usermode */
	assert((t->task.cs & GDT_CPL_MASK) == GDT_CPL_RING3);
	assert((t->task.ss & GDT_CPL_MASK) == GDT_CPL_RING3);

	return dispatchInterrupt(interrupt, error, t);
}

/**
 * @brief Puts the processor into idle state
 * @details This function puts the processor into idle state and waits for the next IRQ. As soon
 *			as the next interrupt was handled this function returns and IRQs are disabled again.
 */
void __attribute__((cdecl)) tssKernelIdle();
asm(".text\n.align 4\n"
".globl tssKernelIdle\n"
"tssKernelIdle:\n"
"__kernelIdleBegin:\n"
"	sti\n"
".loop:\n"
"	hlt\n"
"	jmp .loop\n"
"__kernelIdleEnd:\n"
"	ret\n"
);

/**
 * @}
 */
