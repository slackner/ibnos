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

#ifndef _H_GDT_
#define _H_GDT_

#ifdef __KERNEL__

	#include <stdint.h>

	#include <hardware/context.h>
	#include <process/thread.h>

	/**
	 * \addtogroup GDT
	 * @{
	 *
	 * @name GDT Privileges
	 * @{
	 */
	#define GDT_CPL_MASK		3 /**< Number of bits needed to store the privelege level */
	#define GDT_CPL_RING0		0 /**< Ring 0 */
	#define GDT_CPL_RING3		3 /**< Ring 3 */
	/**
	 * @}
	 *
	 * @name Interrupt types
	 * @{
	 * For more information take a look at http://wiki.osdev.org/Interrupt_Descriptor_Table
	 */
	#define INT_TYPE_TASK32		0x5 /**< Identifies a 32 bit Interrupt Task */
	#define INT_TYPE_INT16		0x6 /**< Identifies a 16 bit Interrupt Task */
	#define INT_TYPE_TRAP16		0x7 /**< Identifies a 16 bit Interrupt Trap */
	#define INT_TYPE_INT32		0xE /**< Identifies a 32 bit standard Interrupt */
	#define INT_TYPE_TRAP32		0xF /**< Identifies a 32 bit Interrupt Trap */
	/**
	 * @}
	 */

	/** Size of the ring0 kernel stack which is mapped into each process */
	#define KERNELSTACK_SIZE	PAGE_SIZE

	/**
	 * @name GDT Size
	 * @{
	 */
	/** Size of the Global Descriptor Table */
	#define GDT_MAX_SIZE		0x10000
	/** Number of pages occupied by the GDT */
	#define GDT_MAX_PAGES		((GDT_MAX_SIZE + PAGE_MASK) >> PAGE_BITS)
	/** Maximum number of entries in the GDT */
	#define GDT_MAX_COUNT		(GDT_MAX_SIZE / sizeof(struct GDTEntry))
	/**
	 * @}
	 *
	 * @name IDT Size
	 * @{
	 */
	/** Size of the Interrupt Descriptor Table */
	#define IDT_MAX_SIZE		PAGE_SIZE
	/** Number of entries in the IDT */
	#define IDT_MAX_COUNT		256
	/**
	 * @}
	 * @}
	 */

	/* fixed addresses in virtual address space */
	#define USERMODE_KERNELSTACK_ADDRESS	0x200000
	#define USERMODE_GDT_ADDRESS			0x201000
	#define USERMODE_IDT_ADDRESS			0x211000
	#define USERMODE_INTJMP_ADDRESS			0x212000
	#define USERMODE_TASK_ADDRESS			0x213000

	#define USERMODE_KERNELSTACK_LIMIT		(USERMODE_KERNELSTACK_ADDRESS + KERNELSTACK_SIZE)

	/* dispatcher ring0 functions */
	#define USERMODE_INTJMP_ENABLE_FPU		(USERMODE_INTJMP_ADDRESS + 2048)

	/* memory locations filled by GDT functions */
	extern void *kernelStack;

	extern void *intJmpTable_kernel;
	extern void *intJmpTable_user;

	extern struct GDTEntry *codeRing0;
	extern struct GDTEntry *dataRing0;
	extern struct GDTEntry *codeRing3;
	extern struct GDTEntry *dataRing3;

	extern struct GDTEntry *kernelTask;
	extern struct GDTEntry *usermodeTask;

	/**
	 * \addtogroup GDT
	 * @{
	 */
	struct GDTTable
	{
		uint16_t limit;
		uint32_t address;
	} __attribute__((packed));

	struct GDTEntry
	{
		uint16_t limit1;
		uint16_t address1;
		uint8_t  address2;
		union {
			uint8_t	access;
			struct {
				uint8_t accessed	: 1;
				uint8_t readWrite	: 1;
				uint8_t dc			: 1;
				uint8_t execute		: 1;
				uint8_t isSystem	: 1;
				uint8_t privlevel	: 2;
				uint8_t present		: 1;
			} accessBits;
		};
		uint8_t limit2		: 4;
		uint8_t user		: 1;
		uint8_t reserved	: 1;
		uint8_t is32bit		: 1;
		uint8_t granularity : 1;
		uint8_t address3;
	} __attribute__((packed));

	struct IDTTable
	{
		uint16_t limit;
		uint32_t address;
	} __attribute__((packed));

	struct IDTEntry
	{
		uint16_t addressLow;
		union{
			uint16_t csSelector;
			uint16_t taskSelector;
		};
		uint8_t zero;
		union{
			uint8_t typeAttr;
			struct {
				uint8_t type			: 4;
				uint8_t storageSegment	: 1;
				uint8_t dpl				: 2;
				uint8_t present			: 1;
			} typeBits;
		};
		uint16_t addressHigh;
	} __attribute__((packed));

	void gdtInit();
	struct GDTEntry* gdtGetFreeEntry();
	uint32_t gdtGetEntryOffset(struct GDTEntry* entry, uint32_t ring);
	void gdtEntrySetAddress(struct GDTEntry* entry, uint32_t address);
	void gdtEntrySetLimit(struct GDTEntry* entry, uint64_t limit);
	void gdtReleaseEntry(struct GDTEntry* entry);

	uint32_t tssRunUsermodeThread(struct thread *t);
	void __attribute__((cdecl)) tssKernelIdle();

	/**
	* @}
	*/
#endif

#endif /* _H_GDT_ */
