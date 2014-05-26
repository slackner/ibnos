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

#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "syscall.h"
#include "vconsole.h"
#include <console/console.h>

#define UNUSED __attribute__((unused))
#define MAX_ARGS 30

char welcomeMSG[] =
	"\n"
	"  _____ ____  _   _    ____   _____ \n"
	" |_   _|  _ \\| \\ | |  / __ \\ / ____|\n"
	"   | | | |_) |  \\| | | |  | | (___  \n"
	"   | | |  _ <| . ` | | |  | |\\___ \\ \n"
	"  _| |_| |_) | |\\  | | |__| |____) |\n"
	" |_____|____/|_| \\_|  \\____/|_____/ \n"
	"\n"
	" Welcome to IBN OS.\n"
	"\n"
	" Type help for a list of available commands.\n";

enum parserState
{
	STATE_BLANK = 0,
	STATE_TEXT,
	STATE_QUOTE,
	STATE_ESCAPE,
	STATE_QUOTE_ESCAPE,
};

static uint32_t shellParseArgs(char *str, char **args, uint32_t maxArgs)
{
	uint32_t count	= 0;
	uint32_t state	= STATE_BLANK;

	/* source and destination pointer (allows inplace unescaping) */
	char *src = str;
	char *dst = str;

	/* completly empty string?*/
	if(*src == 0)
		return 0;

	do
	{
		switch (state)
		{
			case STATE_BLANK:
				if (isspace((int)*src)) continue;
				if (count >= maxArgs) return count;

				if (*src == '|')
				{
					args[count] = NULL;
					count++;
					continue;
				}

				/* save starting position of this argument */
				args[count] = dst;

				if (*src == '\\')
				{
					state = STATE_ESCAPE;
					continue;
				}

				if (*src == '"')
					state = STATE_QUOTE;
				else
					state = STATE_TEXT;

				/* copy one byte to the output */
				if (src != dst) *dst = *src;
				dst++;
				break;

			case STATE_TEXT:
				if (isspace((int)*src) || *src == '|')
				{
					bool isPipe = (*src == '|');

					/* check if it is fully quoted */
					if (*args[count] == '"' && (dst - 1) > args[count] && *(dst - 1) == '"')
					{
						args[count]++;
						*(dst - 1) = 0;
					}

					/* nullterminate the previous string */
					*dst++ = 0;
					count++;

					if (isPipe)
					{
						/* add pipe operator */
						if (count > maxArgs) return count;
						args[count] = NULL;
						count++;
					}

					state = STATE_BLANK;
					continue;
				}

				if (*src == '\\')
				{
					state = STATE_ESCAPE;
					continue;
				}

				if (*src == '"')
					state = STATE_QUOTE;

				/* copy one byte to the output */
				if (src != dst) *dst = *src;
				dst++;
				break;

			case STATE_QUOTE:
				if (*src == '\\')
				{
					state = STATE_QUOTE_ESCAPE;
					continue;
				}

				/* copy one byte to the output */
				if (src != dst) *dst = *src;
				dst++;

				/* it might be possible that some unquoted text follows */
				if (*src == '"')
					state = STATE_TEXT;

				break;

			case STATE_ESCAPE:
			case STATE_QUOTE_ESCAPE:

				if (*src == 'n')
					*dst++ = '\n';
				else if (*src == 'r')
					*dst++ = '\r';
				else if (*src == '"' || *src == ' ' || *src == '|')
					*dst++ = *src;
				else
				{
					/* not a valid escape character */
					*dst++ = '\\';
					*dst++ = *src;
				}

				state = (state == STATE_QUOTE_ESCAPE) ? STATE_QUOTE : STATE_TEXT;
				break;

			default:
				assert(0);
		}
	}
	while (*++src);

	switch (state)
	{
		case STATE_BLANK:
			break;

		case STATE_TEXT:
		case STATE_QUOTE:
			/* check if it is fully quoted */
			if (*args[count] == '"' && (dst - 1) > args[count] && *(dst - 1) == '"')
			{
				args[count]++;
				*(dst - 1) = 0;
			}

			/* nullterminate the previous string */
			*dst++ = 0;
			count++;
			break;

		case STATE_ESCAPE:
		case STATE_QUOTE_ESCAPE:
			*dst++ = '\\';
			*dst++ = 0;
			count++;
			break;

		default:
			assert(0);
	}

	return count;
}

static char *shellInput()
{
	uint32_t pos = 0, length = 1024;
	char *buffer = malloc(length);
	int32_t result;
	char chr;

	while (objectWait(0, 0) >= 0)
	{
		/* buffer is exhausted */
		if (pos + 1 >= length)
		{
			char *new_buffer = realloc(buffer, length + 1024);
			if (!new_buffer)
			{
				free(buffer);
				return NULL;
			}
			buffer  = new_buffer;
			length += 1024;
		}

		/* Note: we cannot read more bytes at once since there is no way to put
		 * them back into the input buffer. Moreover we don't have caching like clib */
		result = objectRead(0, &chr, sizeof(chr));
		if (result < 0) break;
		if (result >= (int32_t)sizeof(chr))
		{
			if (chr == 127)
			{
				if (pos == 0) continue; /* ignore */
				pos--;
			}
			else if (chr == '\e')
				continue; /* ignore */
			else
			{
				/* append at the end of the buffer */
				buffer[pos++] = chr;
			}

			/* write char to stdout */
			objectWrite(1, &chr, sizeof(chr));

			if (chr == '\n')
				break;
		}
	}

	/* add nullterminating character */
	buffer[pos] = 0;
	return buffer;
}

static void shellRunCommand(char **argv)
{
	int32_t out_pipe, in_pipe = 0;
	uint32_t expectedEvents = 0;
	int32_t exitcode = -1;
	int32_t event;

	/* create a new event */
	event = createEvent(true);
	if (event < 0) return;

	while (*argv)
	{
		pid_t pid;

		/* determine number of arguments */
		uint32_t argc = 1;
		while (argv[argc]) argc++;

		/* create output pipe if necessary */
		out_pipe = argv[argc + 1] ? createPipe() : 1;

		pid = fork();
		if (pid < 0) break;

		if (pid == 0) /* child process */
		{
			/* forward stdin/stdout */
			if (in_pipe != 0)  dup2(in_pipe, 0);
			if (out_pipe != 1) dup2(out_pipe, 1);

			execve(argv[0], argv, NULL);
			exit(127);
		}

		/* register notifcation for all processes */
		objectAttach(event, pid, 0, out_pipe);
		objectClose(pid);
		expectedEvents++;

		in_pipe = out_pipe;
		argv += (argc + 1);
	}

	/* wait for termination of all processes */
	while (expectedEvents--)
	{
		out_pipe = objectWait(event, 0);
		if (out_pipe < 0) break;

		if (out_pipe != 1)
		{
			objectShutdown(out_pipe, 1); /* close write */
			objectClose(out_pipe);
		}
		else
		{
			/* get the exitcode */
			exitcode = objectGetStatus(event, 0);
		}

		objectDetach(event, out_pipe);
	}

	if (exitcode == -2)
		printf("*** Programm crashed\n");
	else if (exitcode > 0)
		printf("*** Program terminated with exitcode %d\n", (int)exitcode);
	printf("\n");

	objectClose(event);
}

void shell()
{
	printf("%s\n", welcomeMSG);

	for (;;)
	{
		char *line;

		/* show command prompt */
		printf("> ");
		fflush(stdout);

		/* read a line */
		line = shellInput();
		if (!line) continue;

		/* did the user enter anything at all? */
		if (line[0] != '\n')
		{
			char *args[MAX_ARGS + 2];
			uint32_t count = shellParseArgs(line, args, MAX_ARGS);
			if (count <= 0) break;
			args[count + 1] = args[count] = NULL;
			shellRunCommand(args);
		}

		/* free line again */
		free(line);
	}
}
