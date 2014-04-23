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

#ifndef _H_CONTEXT_
#define _H_CONTEXT_

struct taskContext
{
	uint16_t prevTask;				/* 0x00 */
	uint16_t __res1;

	uint32_t esp0;					/* 0x04 */
	uint16_t ss0;					/* 0x08 */
	uint16_t __res2;

	uint32_t esp1;					/* 0x0C */
	uint16_t ss1;					/* 0x10 */
	uint16_t __res3;

	uint32_t esp2;					/* 0x14 */
	uint16_t ss2;					/* 0x18 */
	uint16_t __res4;

	uint32_t cr3;					/* 0x1C */
	uint32_t eip;					/* 0x20 */
	uint32_t eflags;				/* 0x24 */

	uint32_t eax;					/* 0x28 */
	uint32_t ecx;					/* 0x2C */
	uint32_t edx;					/* 0x30 */
	uint32_t ebx;					/* 0x34 */
	uint32_t esp;					/* 0x38 */
	uint32_t ebp;					/* 0x3C */
	uint32_t esi;					/* 0x40 */
	uint32_t edi;					/* 0x44 */

	uint16_t es;					/* 0x48 */
	uint16_t __res5;
	uint16_t cs;					/* 0x4C */
	uint16_t __res6;
	uint16_t ss;					/* 0x50 */
	uint16_t __res7;
	uint16_t ds;					/* 0x54 */
	uint16_t __res8;
	uint16_t fs;					/* 0x58 */
	uint16_t __res9;
	uint16_t gs;					/* 0x5C */
	uint16_t __res10;

	uint16_t ldt;					/* 0x60 */
	uint16_t __res11;
	uint16_t __res12;				/* 0x64 */
	uint16_t iomap;
} __attribute__((packed));

struct fpuContext
{
	uint16_t controlWord;
	uint16_t __res1;
	uint16_t statusWord;
	uint16_t __res2;
	uint16_t tagWord;
	uint16_t __res3;
	uint32_t codePointer;
	uint16_t codeSegment;
	uint16_t __res4;
	uint32_t dataPointer;
	uint16_t dataSegment;
	uint16_t __res5;
	uint8_t  registerArea[80];
} __attribute__((packed));

#endif /* _H_CONTEXT_ */
