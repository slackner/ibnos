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
#include <assert.h>
#include "vconsole.h"
#include "shell.h"

#define BUFFER_SIZE 1024

extern struct virtConsole consoles[NUM_VIRT_CONSOLES];

int main()
{
	static char buffer[BUFFER_SIZE];
	int32_t i, currentConsole = 0;
	int32_t event = createEvent(true);
	assert (event >= 0);

	/* initializes virtual consoles */
	for (i = 0; i < NUM_VIRT_CONSOLES; i++)
		virtConsoleInit(i);

	/* switch to first consoles */
	virtConsoleSwitchTo(currentConsole);

	/* spawn shell for each terminal */
	for (i = 0; i < NUM_VIRT_CONSOLES; i++)
	{
		consoles[i].pid = fork();
		assert(consoles[i].pid >= 0);

		/* child process */
		if (consoles[i].pid == 0)
		{
			dup2(consoles[i].input,  0);
			dup2(consoles[i].output, 1);
			shell();
		}

		objectAttach(event, consoles[i].output, 0, (uint32_t)consoles[i].output);
	}

	/* also wait for stdin */
	objectAttach(event, 0, 0, 0);

	for (;;)
	{
		int32_t index, length, handle = objectWait(event, 0);
		if (handle < 0) continue;
		length = read(handle, buffer, BUFFER_SIZE);

		/* redirect output from the process to the virtual console */
		if (handle != 0)
		{
			index = 0;
			while (index < NUM_VIRT_CONSOLES && consoles[index].output != handle)
				index++;

			if (index < NUM_VIRT_CONSOLES)
			{
				for (i = 0; i < length; i++){

					if (!consoles[index].escape && buffer[i] == '\e')
					{
						consoles[index].escape = true;
						consoles[index].escapeCode = 0;
						consoles[index].escapeValue = 0;
						continue;
					}

					if (consoles[index].escape)
					{
						if (!consoles[index].escapeCode)
							consoles[index].escapeCode = buffer[i];
						else
						{
							consoles[index].escapeValue = buffer[i];
							virtConsoleProcessEscape(index, (index == currentConsole));
							consoles[index].escape = false;
						}
						continue;
					}
					virtConsolePutChar(index, buffer[i], (currentConsole == index));
				}
			}
			continue;
		}

		/* handle stdin */
		for (i = 0; i < length; i++)
		{
			if (buffer[i] == '\t')
			{
				currentConsole = (currentConsole + 1) % NUM_VIRT_CONSOLES;
				virtConsoleSwitchTo(currentConsole);
				continue;
			}

			/* forward to the child process */
			ibnos_syscall(SYSCALL_OBJECT_WRITE, consoles[currentConsole].input, (uint32_t)&buffer[i], 1);
		}

	};
}
