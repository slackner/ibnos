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

#ifndef _H_UTIL_
#define _H_UTIL_

#ifdef __KERNEL__

	#include <stddef.h>
	#include <stdint.h>
	#include <stdbool.h>

	#include <hardware/context.h>

	#define UNUSED __attribute__((unused))

	uint32_t stringLength(const char *str);
	bool stringIsEqual(const char *str, const char *buf, uint32_t len);

	void *memset(void *ptr, int value, size_t num);
	void *memcpy(void *destination, const void *source, size_t num);
	void *memmove(void *destination, const void *source, size_t num);

	void debugCaptureCpuContext(struct taskContext *context);
	void debugAssertFailed(const char *assertion, const char *file, const char *function, const char *line, struct taskContext *context);
	void debugNotImplemented(const char *file, const char *function, const char *line, struct taskContext *context);

	inline void debugHalt()
	{
		for (;;) asm volatile("cli\nhlt");
	}

	#define _STRINGIZE_DETAIL(x) #x
	#define _STRINGIZE(x) _STRINGIZE_DETAIL(x)

	#define assert(ex) \
		do \
		{ \
			struct taskContext __context; \
			if ((ex)) break; \
			debugCaptureCpuContext(&__context); \
			debugAssertFailed(#ex, __FILE__, __FUNCTION__, _STRINGIZE(__LINE__), &__context); \
		} \
		while(1)

	#define SYSTEM_FAILURE(lines, ...) \
		do \
		{ \
			struct taskContext __context; \
			uint32_t __args[] = {__VA_ARGS__}; \
			memset(&__context, 0xFF, sizeof(__context)); \
			debugCaptureCpuContext(&__context); \
			consoleSystemFailure((lines), sizeof(__args)/sizeof(__args[0]), __args, &__context); \
		} \
		while(1)

	#define NOTIMPLEMENTED() \
		do \
		{ \
			struct taskContext __context; \
			debugCaptureCpuContext(&__context); \
			debugNotImplemented(__FILE__, __FUNCTION__, _STRINGIZE(__LINE__), &__context); \
		} \
		while(1)

#endif

#endif /* _H_UTIL_ */