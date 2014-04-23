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
#include <stdbool.h>

enum escapeCode
{
	ESCAPE_COLOR = 1,
	ESCAPE_CURSORX,
	ESCAPE_CURSORY
};

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define CHAR_OFFSET(x, y) ((y) * VGA_WIDTH + (x))

#define CONSOLE_BLANK MAKE_RAW_CHAR(MAKE_COLOR(COLOR_WHITE, COLOR_BLACK), ' ')

struct virtConsole
{
	uint16_t data[VGA_WIDTH * VGA_HEIGHT];
	uint32_t cursorX;
	uint32_t cursorY;
	int32_t input;
	int32_t output;
	uint8_t color;
	pid_t pid;
	bool escape;
	uint8_t escapeCode;
	uint8_t escapeValue;
};

#define NUM_VIRT_CONSOLES 7

void virtConsoleSwitchTo(uint32_t index);
void virtConsoleInit(uint32_t index);
void virtConsoleScrollUp(uint32_t index);
void virtConsolePutChar(uint32_t index, char chr, bool active);
void virtConsoleProcessEscape(uint32_t index, bool active);