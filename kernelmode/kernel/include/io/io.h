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

#ifndef _H_IO_
#define _H_IO_

#ifdef __KERNEL__

	#include <stdint.h>

	inline void outb(uint16_t port, uint8_t val)
	{
		asm volatile( "outb %0, %1" : : "a"(val), "Nd"(port) );
	}

	inline void outw(uint16_t port, uint16_t val)
	{
		asm volatile( "outw %0, %1" : : "a"(val), "Nd"(port) );
	}

	inline void outl(uint16_t port, uint32_t val)
	{
		asm volatile( "outl %0, %1" : : "a"(val), "Nd"(port) );
	}

	inline uint8_t inb(uint16_t port)
	{
		uint8_t v;
		asm volatile("inb %1,%0" : "=a" (v) : "dN" (port));
		return v;
	}

	inline uint16_t inw(uint16_t port)
	{
		uint16_t v;
		asm volatile("inw %1,%0" : "=a" (v) : "dN" (port));
		return v;
	}

	inline uint32_t inl(uint16_t port)
	{
		uint32_t v;
		asm volatile("inl %1,%0" : "=a" (v) : "dN" (port));
		return v;
	}

#endif

#endif /* _H_IO_ */