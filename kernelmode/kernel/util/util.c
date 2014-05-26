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

#include <util/util.h>
#include <console/console.h>

/**
 * @brief Returns the length of a nullterminated string
 *
 * @param str Pointer to the nullterminated string
 * @return Length in characters (not including the terminating null character)
 */
uint32_t stringLength(const char *str)
{
	uint32_t len = 0;
	if (!str)
		return 0;

	while (*str++)
		len++;

	return len;
}

/**
 * @brief Checks if the string is equal to the memory region
 *
 * @param str Pointer to the nullterminated string
 * @param buf Pointer to a buffer
 * @param len Length of the buffer
 * @return True if both strings are equal, otherwise false
 */
bool stringIsEqual(const char *str, const char *buf, uint32_t len)
{
	/* remove terminating nulls from buf */
	while (len && !buf[len - 1]) len--;

	if (!str)
		return (len == 0);

	while (*str && len && (*str == *buf))
	{
		str++;
		buf++;
		len--;
	}

	/* both strings are equal if this was the last character */
	return (!len && !*str);
}

/**
 * @brief Converts an octal string to a integer number
 * 
 * @param str Pointer to a string
 * @param len Length of the buffer
 * 
 * @return Parsed integer number
 */
uint32_t stringParseOctal(const char *str, uint32_t len)
{
	uint32_t value = 0;

	/* skip over leading whitespace */
	while (len && *str == ' ')
	{
		str++;
		len--;
	}

	while (len && (*str >= '0' && *str < '8'))
	{
		value = (value << 3) | (*str - '0');
		str++;
		len--;
	}

	/* skip over trailing whitespace */
	while (len && *str == ' ')
	{
		str++;
		len--;
	}

	/* not a nulltermination */
	if (len && *str) return -1;

	return value;
}

/**
 * @brief Fills a memory region with some specific byte value
 *
 * @param ptr Pointer to the start of the memory region
 * @param value Value used to fill the memory region
 * @param num Length of the memory region in bytes
 * @return ptr
 */
void *memset(void *ptr, int value, size_t num)
{
	uint8_t *dst = ptr;
	while (num--) *dst++ = value;
	return ptr;
}

/**
 * @brief Copies a block of memory from source to destination
 * @details The source and destination blocks shouldn't be overlapping, otherwise
 *			it is not safe to call this function. If the regions could be overlapping
 *			please use memmove() instead.
 *
 * @param destination Pointer to the destination memory region
 * @param source Pointer to the source memory region
 * @param num Length of the memory region to copy, in bytes
 * @return destination
 */
void *memcpy(void *destination, const void *source, size_t num)
{
	uint8_t *dst = destination;
	const uint8_t *src = source;
	while (num--) *dst++ = *src++;
	return destination;
}

/**
 * @brief Moves a block of memory from source to destination
 * @details Similar to memcpy(), but will also work safely if both source and destination
 *			regions are overlapping.
 *
 * @param destination Pointer to the destination memory region
 * @param source Pointer to the source memory region
 * @param num Length of the memory region to copy, in bytes
 * @return destination
 */
void *memmove(void *destination, const void *source, size_t num)
{
	if (destination < source || destination >= source + num)
		return memcpy(destination, source, num);
	else if (destination != source)
	{
		uint8_t *dst = (uint8_t *)destination + num - 1;
		uint8_t *src = (uint8_t *)source + num - 1;
		while (num--) *dst-- = *src--;
	}
	return destination;
}

/**
 * @brief Fills out the task context structure with the values from the currently
 *		  running code
 * @details This function will capture all registers, segment registers, and control
 *			registers and save these values to the task context structure. The stack
 *			pointer and EIP will be adjusted, so that they point immediately after this
 *			function returns.
 *
 * @param context Pointer to a task context structure
 */
void debugCaptureCpuContext(struct taskContext *context);
asm(".text\n.align 4\n"
".globl debugCaptureCpuContext\n"
"debugCaptureCpuContext:\n"
"	pushl %eax\n"
"	movl 8(%esp), %eax\n"
"\n"
"	popl 0x28(%eax)\n"
"	movl %ecx, 0x2C(%eax)\n"
"	movl %edx, 0x30(%eax)\n"
"	movl %ebx, 0x34(%eax)\n"
"	leal 4(%esp), %edx\n"
"	movl %edx, 0x38(%eax)\n"
"	movl %ebp, 0x3C(%eax)\n"
"	movl %esi, 0x40(%eax)\n"
"	movl %edi, 0x44(%eax)\n"
"\n"
"	movw %es, 0x48(%eax)\n"
"	movw %cs, 0x4C(%eax)\n"
"	movw %ss, 0x50(%eax)\n"
"	movw %ds, 0x54(%eax)\n"
"	movw %fs, 0x58(%eax)\n"
"	movw %gs, 0x5C(%eax)\n"
"\n"
"	movl %cr3, %edx\n"
"	movl %edx, 0x1C(%eax)\n"
"	movl (%esp), %edx\n"
"	movl %edx, 0x20(%eax)\n"
"	pushfl\n"
"	popl 0x24(%eax)\n"
"\n"
"	ret\n"
);

/**
 * @brief Used internally to implement assert()
 * @details This function shows an assertion message. Normally it shouldn't be
 *			necessary to call this function manually, all values will automatically
 *			be filled out by using the assert() macro from util.h.
 *
 * @param assertion String containing the condition which failed
 * @param file File in which the assertion occured
 * @param function Function in which the assertion occured
 * @param line String representing the line number, where the assertion occured
 * @param context Task context structure containing the CPU registers after the error occured
 */
void debugAssertFailed(const char *assertion, const char *file, const char *function, const char *line, struct taskContext *context)
{
	const char *lines[] =
	{
		" ASSERTION FAILED ",
		  "  Assertion: ", assertion,
		"\n  File:      ", file,
		"\n  Function:  ", function,
		"\n  Line:      ", line,
		NULL
	};

	consoleSystemFailure(lines, 0, NULL, context);
}

/**
 * @brief Used internally to implement NOTIMPLEMENTED()
 *
 * @param file File in which the feature is missing
 * @param function Function in which the assertion occured
 * @param line String representing the line number, where the assertion occured
 * @param context Task context structure containing the CPU registers
 */
void debugNotImplemented(const char *file, const char *function, const char *line, struct taskContext *context)
{
	const char *lines[] =
	{
		" NOT IMPLEMENTED ",
		  "  Unimplemented code section reached.",
		"\n",
		"\n  File:      ", file,
		"\n  Function:  ", function,
		"\n  Line:      ", line,
		NULL
	};

	consoleSystemFailure(lines, 0, NULL, context);
}
