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

#include <multiboot/multiboot.h>
#include <console/console.h>

#include <memory/physmem.h>
#include <memory/paging.h>
#include <memory/allocator.h>

#include <hardware/gdt.h>
#include <hardware/pic.h>
#include <hardware/keyboard.h>
#include <hardware/pit.h>

#include <process/object.h>
#include <process/process.h>
#include <process/thread.h>
#include <process/pipe.h>
#include <process/timer.h>
#include <process/filesystem.h>
#include <process/handle.h>
#include <process/timer.h>

#include <loader/elf.h>

#include <util/list.h>
#include <util/util.h>

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

/**
 * @brief Initializes the FPU related bits in the CR0 register
 */
void fpuInit()
{
	uint32_t cr0 = __getCR0();
	cr0 &= ~(1 << 2); /* disable EMuleration */
	cr0 |=  (1 << 5); /* enable Native Exception */
	cr0 |=  (1 << 3); /* enable TS bit (kernel shouldn't use FPU) */
	cr0 |=  (1 << 1); /* enable MP */
	__setCR0(cr0);
}

/**
 * @brief Spawns a new process, and afterwards loads the ELF module into it
 * @details The provided memory address and length should represent an ELF
 *			module which is loaded into the kernel memory. This function will
 *			spawn a new process, copy the ELF sections into newly allocated
 *			memory, and create the main thread which is initialized to start
 *			with the entry point.
 *
 * @param addr Address pointing to the begin of the ELF module
 * @param length Length of the ELF module in bytes
 *
 * @return Kernel process object
 */
struct process *loadELFModule(void *addr, uint32_t length)
{
	struct process *p;
	struct thread *t = NULL;

	p = processCreate(NULL);
	if (p)
	{
		/* load ELF executable */
		assert(elfLoadBinary(p, addr, length));

		t = threadCreate(p, NULL, p->entryPoint);
		if (t) objectRelease(t);

		objectRelease(p);
	}

	return (t != NULL) ? p : NULL;
}

/**
 * @brief Kernel main entry point
 * @details This function is the kernel main entry point, which is called after
 *			GRUB has loaded the kernel into the physical memory address specified
 *			in the linker script (linker.ld). The passed argument contains information
 *			about the kernel command line arguments and the memory layout returned
 *			by the BIOS. This function will never return.
 *
 * @param bootInfo Bootinfo structure filled by the GRUB bootloader
 */
void kernel_main(multiboot_info_t *bootInfo)
{
	multiboot_module_t *module;
	struct process *initProcess;
	struct stdout *stdout;
	struct pipe *stdin;

	consoleClear();

	assert(sizeof(struct taskContext) == 0x68);
	assert(sizeof(struct fpuContext) == 0x6C);
	assert(offsetof(struct taskContext, eip) == 0x20);

	/* ensure that GRUB loaded a usermode process */
	assert((bootInfo->flags & MULTIBOOT_INFO_MODS));
	assert(bootInfo->mods_count >= 2);

	/* ensure that entry is valid */
	module = (multiboot_module_t *)bootInfo->mods_addr;
	assert(module[0].mod_start && module[0].mod_start < module[0].mod_end);
	assert(module[1].mod_start && module[1].mod_start < module[1].mod_end);

	/* Initialize kernel modules - NOTE: the order is important */
	physMemInit(bootInfo);
	consoleInit();
	consoleSetFont();
	physMemProtectBootEntry(0x9000, PAGE_SIZE);
	pagingInit();
	gdtInit();
	fpuInit();

	/* initialize stdin & stdout */
	stdout	= stdoutCreate();
	stdin	= pipeCreate();

	picInit(0x20);
	keyboardInit(&stdin->obj);
	timerInit();
	fileSystemInit((void*)module[1].mod_start, module[1].mod_end - module[1].mod_start);

	/* load init process */
	initProcess = loadELFModule((void*)module[0].mod_start, module[0].mod_end - module[0].mod_start);
	assert(initProcess);

	/* initialize stdout, stdin & stderr handles */
	handleSet(&initProcess->handles, 0, &stdin->obj);
	handleSet(&initProcess->handles, 1, &stdout->obj);
	handleSet(&initProcess->handles, 2, &stdout->obj);

	/* schedule threads */
	threadSchedule();

	/* the last process was terminated */
	consoleWriteString("\n\nThe last process was terminated. You can now safely reboot your computer.\n");
	debugHalt();
}
