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

#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <process/process.h>

#define UNUSED __attribute__((unused))
#define PAGE_SIZE 0x1000

static inline char *__byteSizeMem(unsigned int size, char *buf, int length)
{
	if (size < 1024 * 5)
		snprintf(buf, length, "%u B", size);
	else if (size < 1024 * 1024 * 5)
		snprintf(buf, length, "%u KB", size / 1024);
	else
		snprintf(buf, length, "%u MB", size / (1024 * 1024));

	return buf;
}

int main(UNUSED int argc, UNUSED char **argv)
{
	struct processInfo processInfo[1024], *info;
	unsigned int numProcesses;

	numProcesses = getProcessInfo(processInfo, sizeof(processInfo)/sizeof(processInfo[0]));

	if (numProcesses > sizeof(processInfo)/sizeof(processInfo[0]))
	{
		numProcesses = sizeof(processInfo)/sizeof(processInfo[0]);
		printf("Too many processes, will only display information for the first %u\n\n", numProcesses);
	}
	else
		printf("%u running processes\n\n", numProcesses);

	printf("%8s | %4s | %4s | %7s | %7s | %7s | %7s | %7s | %4s\n",
		"PID", "THRD", "WAIT", "SHR MEM", "FRK MEM", "RSV MEM", "OUT MEM", "PHS MEM", "HNDL");
	printf("--------------------------------------------------------------------------------");

	info = processInfo;
	while (numProcesses)
	{
		char buf[5][34];

		printf("%08x | %4u | %4u | %7s | %7s | %7s | %7s | %7s | %4u\n",
			(unsigned int)info->processID,
			(unsigned int)info->numberOfTotalThreads,
			(unsigned int)info->numberOfBlockedThreads,
			__byteSizeMem(info->pagesShared * PAGE_SIZE,   buf[0], sizeof(buf[0])),
			__byteSizeMem(info->pagesNoFork * PAGE_SIZE,   buf[1], sizeof(buf[1])),
			__byteSizeMem(info->pagesReserved * PAGE_SIZE, buf[2], sizeof(buf[2])),
			__byteSizeMem(info->pagesOutpaged * PAGE_SIZE, buf[3], sizeof(buf[3])),
			__byteSizeMem(info->pagesPhysical * PAGE_SIZE, buf[4], sizeof(buf[4])),
			(unsigned int)info->handleCount
		);
		numProcesses--;
		info++;
	}

	return 0;
}