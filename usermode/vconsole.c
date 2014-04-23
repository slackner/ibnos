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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syscall.h>
#include <unistd.h>
#include <console/console.h>
#include <assert.h>
#include "vconsole.h"

struct virtConsole consoles[NUM_VIRT_CONSOLES];

void virtConsoleSwitchTo(uint32_t index)
{
	consoleSetFlags(CONFLAGS_HWCURSOR_AUTO | CONFLAGS_RAW_MODE);
	consoleSetCursor(0, 0);
	consoleWriteRaw(consoles[index].data, VGA_WIDTH * VGA_HEIGHT);
	consoleSetCursor(consoles[index].cursorX, consoles[index].cursorY);
	consoleSetHardwareCursor(consoles[index].cursorX, consoles[index].cursorY);
	consoleSetFlags(CONFLAGS_HWCURSOR | CONFLAGS_HWCURSOR_AUTO | CONFLAGS_RAW_MODE);
}

void virtConsoleInit(uint32_t index)
{
	uint32_t i;
	char text[VGA_WIDTH];
	assert(index < NUM_VIRT_CONSOLES);

	memset(text, 0, VGA_WIDTH);
	snprintf(text, VGA_WIDTH, "IBN OS - Console %d", (int)index);

	/* initialize the terminal with a black background */
	for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
		consoles[index].data[i] = CONSOLE_BLANK;

	/* print header line */
	for (i = 0; i < VGA_WIDTH; i++)
		consoles[index].data[i] = MAKE_RAW_CHAR(MAKE_COLOR(COLOR_WHITE, COLOR_BLUE), text[i] ? text[i] : ' ');

	consoles[index].cursorX = 0;
	consoles[index].cursorY = 1;
	consoles[index].input  = createPipe();
	consoles[index].output = createPipe();
	consoles[index].escape = false;
	consoles[index].escapeCode  = 0;
	consoles[index].escapeValue = 0;
	consoles[index].color = MAKE_COLOR(COLOR_WHITE, COLOR_BLACK);
	assert(consoles[index].input  >= 0);
	assert(consoles[index].output >= 0);
}

void virtConsoleScrollUp(uint32_t index)
{
	int32_t x, y;
	assert(index < NUM_VIRT_CONSOLES);

	for (y = 2; y < VGA_HEIGHT; y++)
	{
		for (x = 0; x < VGA_WIDTH; x++)
			consoles[index].data[CHAR_OFFSET(x, y-1)] = consoles[index].data[CHAR_OFFSET(x, y)];
	}

	for (x = 0; x < VGA_WIDTH; x++)
		consoles[index].data[CHAR_OFFSET(x, VGA_HEIGHT - 1)] = CONSOLE_BLANK;
}

void virtConsolePutChar(uint32_t index, char chr, bool active)
{
	int32_t x = consoles[index].cursorX; /* must be signed */
	int32_t y = consoles[index].cursorY; /* must be signed */
	assert(index < NUM_VIRT_CONSOLES);

	if (chr == 127)
	{
		x--;

		if (x < 0)
		{
			x = VGA_WIDTH - 1;
			y--;
		}

		if (y < 1)
			y = 1;

		consoles[index].data[CHAR_OFFSET(x, y)] = MAKE_RAW_CHAR(consoles[index].color, ' ');
		if (active) virtConsoleSwitchTo(index);
	}
	else
	{
		if (chr != '\n')
		{
			consoles[index].data[CHAR_OFFSET(x, y)] = MAKE_RAW_CHAR(consoles[index].color, chr);
			if (active) consoleWriteRaw(&consoles[index].data[CHAR_OFFSET(x, y)], 1);
			x++;
		}
		else
		{
			y++;
			x = 0;
		}

		if (x >= VGA_WIDTH)
		{
			x = 0;
			y++;
		}

		if (y >= VGA_HEIGHT)
		{
			y = VGA_HEIGHT - 1;
			virtConsoleScrollUp(index);
			consoles[index].cursorX = x;
			consoles[index].cursorY = y;
			if (active) virtConsoleSwitchTo(index);
			return;
		}
	}

	consoles[index].cursorX = x;
	consoles[index].cursorY = y;
	if (active)
	{
		consoleSetCursor(x, y);
		consoleSetHardwareCursor(x, y);
	}
}

void virtConsoleProcessEscape(uint32_t index, bool active)
{
	if (!consoles[index].escape)
		return;

	if (consoles[index].escapeCode == ESCAPE_COLOR)
		consoles[index].color = consoles[index].escapeValue;
	else if(consoles[index].escapeCode == ESCAPE_CURSORX || consoles[index].escapeCode == ESCAPE_CURSORY)
	{
		uint32_t posX = consoles[index].cursorX;
		uint32_t posY = consoles[index].cursorY;

		if (consoles[index].escapeCode == ESCAPE_CURSORX)
			posX = consoles[index].escapeValue;
		if (consoles[index].escapeCode == ESCAPE_CURSORY)
			posY = consoles[index].escapeValue + 1;

		if (posX < VGA_WIDTH && posY < VGA_HEIGHT)
		{
			consoles[index].cursorX = posX;
			consoles[index].cursorY = posY;

			if (active)
			{
				consoleSetCursor(posX, posY);
				consoleSetHardwareCursor(posX, posY);
			}
		}
	}
}