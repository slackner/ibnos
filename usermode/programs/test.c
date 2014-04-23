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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syscall.h>
#include <unistd.h>
#include <console/console.h>
#include <process/timer.h>
#include "../vconsole.h"

#define UNUSED __attribute__((unused))

static uint32_t __failureSemaphore;
static const char *__currentTest;

#define DECLARE_TEST_FUNC(func) \
	void __test__ ## func(); \
	void test_ ## func() \
	{ \
		uint32_t failures = 0; \
		pid_t child = fork(); \
		__currentTest = #func; \
		if (!child) \
		{ \
			__test__ ## func(); \
			exit(0); \
		} \
		else if (child > 0) \
		{ \
			if (ibnos_syscall(SYSCALL_OBJECT_WAIT, child, 0) != 0) failures++; \
			ibnos_syscall(SYSCALL_OBJECT_CLOSE, child); \
		} \
		failures += ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, __failureSemaphore, 0); \
		printf("\e%c%c", ESCAPE_COLOR, MAKE_COLOR(failures ? COLOR_LIGHT_RED : COLOR_LIGHT_GREEN, COLOR_BLACK)); \
		printf("%s: Test %s with %u failures\n", #func, failures ? "failed" : "succeeded", (unsigned int)failures); \
		printf("\e%c%c", ESCAPE_COLOR, MAKE_COLOR(COLOR_WHITE, COLOR_BLACK)); \
		if (failures) exit(1); \
	} \
	void __test__ ## func()

#define _STRINGIZE_DETAIL(x) #x
#define _STRINGIZE(x) _STRINGIZE_DETAIL(x)

#define ok(ex) \
	do \
	{ \
		if ((ex)) break; \
		printf("\e%c%c", ESCAPE_COLOR, MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK)); \
		printf("%s/%s: Test %s failed\n", __currentTest, _STRINGIZE(__LINE__), #ex); \
		printf("\e%c%c", ESCAPE_COLOR, MAKE_COLOR(COLOR_WHITE, COLOR_BLACK)); \
		ibnos_syscall(SYSCALL_OBJECT_SIGNAL, __failureSemaphore, 0); \
	} \
	while(0)

static uint32_t thread_child_thread(uint32_t a, uint32_t b, uint32_t c)
{
	uint32_t i;
	int32_t child;

	ok(b == 2);
	ok(c == 3);

	if (a == 1)
	{
		for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);
		return 42;
	}
	else if (a == 2)
	{
		child = ibnos_syscall(SYSCALL_GET_CURRENT_THREAD);
		ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child, 0) == 43);
		for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);
	}

	ok(0); /* should never be reached */
	return 0;
}

DECLARE_TEST_FUNC(thread)
{
	uint32_t i;
	int32_t child;

	child = createThread(&thread_child_thread, 1, 2, 3);
	ok(child >= 0);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, child, 0) == -1);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child, 0) == 42);
	ok(ibnos_syscall(SYSCALL_OBJECT_CLOSE, child));

	child = createThread(&thread_child_thread, 2, 2, 3);
	ok(child >= 0);
	for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);
	ok(ibnos_syscall(SYSCALL_OBJECT_SIGNAL, child, 43));
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, child, 0) == -1);
	ok(ibnos_syscall(SYSCALL_OBJECT_SHUTDOWN, child, 44));
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, child, 0) == 44);
}

DECLARE_TEST_FUNC(process)
{
	int32_t child1, child2;
	uint32_t i;

	child1 = fork();
	ok(child1 >= 0);

	if (!child1)
	{
		for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);
		exit(72);
	}

	child2 = fork();
	ok(child2 >= 0);

	if (!child2)
	{
		ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child1, 0) == 72);
		exit(73);
	}

	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, child1, 0) == -1);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, child2, 0) == -1);

	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child1, 0) == 72);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child2, 0) == 73);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, child1, 0) == 72);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, child2, 0) == 73);

	ok(ibnos_syscall(SYSCALL_OBJECT_CLOSE, child1));
	ok(ibnos_syscall(SYSCALL_OBJECT_CLOSE, child2));

	child1 = fork();
	ok(child1 >= 0);

	if (!child1)
	{
		asm volatile("hlt");
		ok(0);
		exit(13);
	}

	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child1, 0) == -2);
	ok(ibnos_syscall(SYSCALL_OBJECT_CLOSE, child1));
}

static int32_t semaphore1;
static int32_t semaphore2;

static uint32_t semaphore_child_thread()
{
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, semaphore1, 0) == 1);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, semaphore2, 0) == 3);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, semaphore1, 0) == 0);

	/* this call will be blocking because the semaphore has a counter of zero */
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, semaphore1, 0) == 0);
	ok(ibnos_syscall(SYSCALL_OBJECT_SIGNAL, semaphore2, 0));
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, semaphore2, 0) == 4);

	/* this call will be blocking because the semaphore has a counter of zero */
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, semaphore1, 0) == 13);

	return 42;
}

DECLARE_TEST_FUNC(semaphore)
{
	int32_t child;
	uint32_t i;

	semaphore1 = ibnos_syscall(SYSCALL_CREATE_SEMAPHORE, 2);
	semaphore2 = ibnos_syscall(SYSCALL_CREATE_SEMAPHORE, 3);
	ok(semaphore1 >= 0 && semaphore2 >= 0);

	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, semaphore1, 0) == 2);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, semaphore2, 0) == 3);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, semaphore1, 0) == 1);

	child = createThread(&semaphore_child_thread, 0, 0, 0);
	ok(child >= 0);

	/* parent process */
	for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, semaphore2, 0) == 3);
	ok(ibnos_syscall(SYSCALL_OBJECT_SIGNAL, semaphore1, 0));

	for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, semaphore2, 0) == 4);
	ok(ibnos_syscall(SYSCALL_OBJECT_SIGNAL, child, 13));

	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child, 0) == 42);
}

static uint32_t pipe_child_thread(uint32_t pipe)
{
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, pipe, 0) == 64);
	while (ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 0) < 0x10000)
		ibnos_syscall(SYSCALL_YIELD);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, pipe, 1) == 64);
	return 44;
}

DECLARE_TEST_FUNC(pipe)
{
	static const char teststr[] = "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
	char buffer[64];
	uint32_t i;
	int32_t pipe, child;

	pipe = ibnos_syscall(SYSCALL_CREATE_PIPE);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 0) == 0);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 1) == 0x10000);

	child = createThread(&pipe_child_thread, pipe, 0, 0);
	ok(child >= 0);
	for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);

	for (i = 0; i < 0x10000 / (sizeof(teststr)-1); i++)
	{
		ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 0) == (signed)(i * (sizeof(teststr)-1)));
		ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 1) == (signed)(0x10000 - i * (sizeof(teststr)-1)));
		ok(ibnos_syscall(SYSCALL_OBJECT_WRITE, pipe, (uint32_t)teststr, sizeof(teststr)-1) == sizeof(teststr)-1);
	}

	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 0) == 0x10000);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 1) == 0);
	for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);

	memset(buffer, 0, sizeof(buffer));
	ok(ibnos_syscall(SYSCALL_OBJECT_READ, pipe, (uint32_t)buffer, sizeof(buffer)) == sizeof(buffer));
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 0) == 0x10000 - sizeof(buffer));
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 1) == sizeof(buffer));
	ok(memcmp(buffer, teststr, sizeof(buffer)) == 0);

	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, pipe, 0) == 0x10000 - sizeof(buffer));
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, pipe, 1) == sizeof(buffer));
	for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);

	memset(buffer, 'X', sizeof(buffer));
	ok(ibnos_syscall(SYSCALL_OBJECT_WRITE, pipe, (uint32_t)buffer, sizeof(buffer)) == sizeof(buffer));

	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 0) == 0x10000);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 1) == 0);

	for (i = 1; i < 0x10000 / (sizeof(teststr)-1); i++)
	{
		memset(buffer, 0, sizeof(buffer));
		ok(ibnos_syscall(SYSCALL_OBJECT_READ, pipe, (uint32_t)buffer, sizeof(buffer)) == sizeof(buffer));
		ok(memcmp(buffer, teststr, sizeof(buffer)) == 0);
	}

	memset(buffer, 0, sizeof(buffer));
	ok(ibnos_syscall(SYSCALL_OBJECT_READ, pipe, (uint32_t)buffer, sizeof(buffer)) == sizeof(buffer));
	for (i = 0; i < sizeof(buffer); i++)
		if (buffer[i] != 'X') ok(0);

	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 0) == 0);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 1) == 0x10000);

	memset(buffer, 'O', sizeof(buffer));
	ok(ibnos_syscall(SYSCALL_OBJECT_WRITE, pipe, (uint32_t)buffer, sizeof(buffer)) == sizeof(buffer));

	ok(ibnos_syscall(SYSCALL_OBJECT_SHUTDOWN, pipe, 1));
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 0) == sizeof(buffer));
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 1) == -1);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, pipe, 0) == sizeof(buffer));
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, pipe, 1) == -1);

	ok(ibnos_syscall(SYSCALL_OBJECT_SHUTDOWN, pipe, 0));
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 0) == -1);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, pipe, 1) == -1);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, pipe, 0) == -1);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, pipe, 1) == -1);

	ok(ibnos_syscall(SYSCALL_OBJECT_CLOSE, pipe));

	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child, 0) == 44);
	ok(ibnos_syscall(SYSCALL_OBJECT_CLOSE, child));
}

DECLARE_TEST_FUNC(timer)
{
	uint32_t timer;
	struct timerInfo info;
	uint32_t totalTime;

	timer = ibnos_syscall(SYSCALL_CREATE_TIMER, 0);

	totalTime = ibnos_syscall(SYSCALL_GET_MONOTONIC_CLOCK);

	info.timeout = 1;
	info.interval = 1;
	ok(ibnos_syscall(SYSCALL_OBJECT_WRITE, timer, (uint32_t)&info, sizeof(info)) == sizeof(info));
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, timer, 0) >= 12);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, timer, 0) == -1);

	info.timeout = 15;
	info.interval = 15;
	ok(ibnos_syscall(SYSCALL_OBJECT_WRITE, timer, (uint32_t)&info, sizeof(info)) == sizeof(info));
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, timer, 0) == 1);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, timer, 0) == 1);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, timer, 0) == 1);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, timer, 0) == -1);

	info.timeout = 30;
	info.interval = 30;
	ok(ibnos_syscall(SYSCALL_OBJECT_WRITE, timer, (uint32_t)&info, sizeof(info)) == sizeof(info));
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, timer, 0) == 1);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, timer, 0) == 1);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, timer, 0) == 1);
	ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, timer, 0) == -1);

	info.timeout = 30;
	info.interval = 0;
	ok(ibnos_syscall(SYSCALL_OBJECT_WRITE, timer, (uint32_t)&info, sizeof(info)) == sizeof(info));
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, timer, 0) == 1);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, timer, 0) == 0);

	totalTime = ibnos_syscall(SYSCALL_GET_MONOTONIC_CLOCK) - totalTime;
	ok(totalTime >= 12 + 3 * 15 + 3 * 30 + 30);
}

DECLARE_TEST_FUNC(event)
{
	int32_t child1, child2;
	int32_t sem1, sem2;
	int32_t event;
	uint32_t i;

	event = ibnos_syscall(SYSCALL_CREATE_EVENT, 1);

	child1 = fork();
	ok(child1 >= 0);

	if (!child1)
	{
		ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, event, 0) == 100);
		exit(13);
	}

	child2 = fork();
	ok(child2 >= 0);

	if (!child2)
	{
		ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, event, 0) == 100);
		exit(14);
	}

	for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);
	ok(ibnos_syscall(SYSCALL_OBJECT_SIGNAL, event, 100));

	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child1, 0) == 13);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child2, 0) == 14);
	ok(ibnos_syscall(SYSCALL_OBJECT_CLOSE, child1));
	ok(ibnos_syscall(SYSCALL_OBJECT_CLOSE, child2));

	sem1 = ibnos_syscall(SYSCALL_CREATE_SEMAPHORE, 3);
	ok(sem1 >= 0);

	sem2 = ibnos_syscall(SYSCALL_CREATE_SEMAPHORE, 2);
	ok(sem2 >= 0);

	ok(ibnos_syscall(SYSCALL_OBJECT_ATTACH_OBJ, event, sem1, 0, 91));
	ok(ibnos_syscall(SYSCALL_OBJECT_ATTACH_OBJ, event, sem2, 0, 92));

	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, event) == 91);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, event) == 91);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, event) == 91);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, event) == 92);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, event) == 92);

	ok(ibnos_syscall(SYSCALL_OBJECT_DETACH_OBJ, event, 92));
	ok(ibnos_syscall(SYSCALL_OBJECT_ATTACH_OBJ, event, sem2, 0, 93));

	child2 = fork();
	ok(child2 >= 0);

	if (!child2)
	{
		ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, ibnos_syscall0(SYSCALL_GET_CURRENT_PROCESS), 0));
		ok(0);
		exit(51);
	}

	ok(ibnos_syscall(SYSCALL_OBJECT_ATTACH_OBJ, event, child2, 0, 77));

	child1 = fork();
	ok(child1 >= 0);

	if (!child1)
	{
		ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, event, 0) == 93);
		ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, event, 0) == 91);
		ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, event, 0) == 77);
		ok(ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, event, 0) == 15);
		ok(ibnos_syscall(SYSCALL_OBJECT_DETACH_OBJ, event, 77));
		ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, event, 0) == 93);
		exit(94);
	}

	ok(ibnos_syscall(SYSCALL_OBJECT_SIGNAL, sem2, 0));
	for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);
	ok(ibnos_syscall(SYSCALL_OBJECT_SIGNAL, sem1, 0));
	for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);
	ok(ibnos_syscall(SYSCALL_OBJECT_SHUTDOWN, child2, 15));
	for (i = 0; i < 100; i++) ibnos_syscall(SYSCALL_YIELD);
	ok(ibnos_syscall(SYSCALL_OBJECT_SIGNAL, sem2, 0));

	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child1, 0) == 94);
	ok(ibnos_syscall(SYSCALL_OBJECT_WAIT, child2, 0) == 15);
	ok(ibnos_syscall(SYSCALL_OBJECT_CLOSE, child1));
	ok(ibnos_syscall(SYSCALL_OBJECT_CLOSE, child2));

	ok(ibnos_syscall(SYSCALL_OBJECT_CLOSE, event));
}

int cmdTest(UNUSED char **argv, UNUSED int argc)
{
	__failureSemaphore = ibnos_syscall(SYSCALL_CREATE_SEMAPHORE, 0);

	test_thread();
	test_process();
	test_semaphore();
	test_pipe();
	test_timer();
	test_event();

	printf("All tests finished.\n");
	exit(0);
}