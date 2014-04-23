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

#ifndef _H_CONSOLE_
#define _H_CONSOLE_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * \addtogroup console
 * @{
 */

/**
 * Text mode colors codes
 */
enum VGA_COLOR
{
	COLOR_BLACK			= 0,
	COLOR_BLUE			= 1,
	COLOR_GREEN			= 2,
	COLOR_CYAN			= 3,
	COLOR_RED			= 4,
	COLOR_MAGENTA		= 5,
	COLOR_BROWN			= 6,
	COLOR_LIGHT_GREY	= 7,
	COLOR_DARK_GREY		= 8,
	COLOR_LIGHT_BLUE	= 9,
	COLOR_LIGHT_GREEN	= 10,
	COLOR_LIGHT_CYAN	= 11,
	COLOR_LIGHT_RED		= 12,
	COLOR_LIGHT_MAGENTA = 13,
	COLOR_LIGHT_BROWN	= 14,
	COLOR_WHITE			= 15,
};

/**
 * Console flags (can be combined)
 */
enum CONSOLE_FLAGS
{
	CONFLAGS_ECHO			= 1, /**< Show keyboard input */
	CONFLAGS_HWCURSOR		= 2, /**< Show hardware cursor */
	CONFLAGS_HWCURSOR_AUTO	= 4, /**< Automatically move hardware cursor */
	CONFLAGS_RAW_MODE		= 8, /**< Do not parse linebreaks or scroll up */
};

/**
 * Combines two 4 bit colors codes into a single byte as required by the
 * VGA card
 */
#define MAKE_COLOR(foreground, background) (((foreground) & 15) | (background) << 4)

/**
 * Combines a color and character code into a packed 16 bit raw character value
 */
#define MAKE_RAW_CHAR(color, chr) ((((uint32_t)(chr)) & 0xFF) | (((uint32_t)(color)) & 0xFF) << 8)

/**
 * Extract the color information from a raw character. Use #BG_COLOR and
 * #FG_COLOR to extract the returned value into background and foreground
 * color.
 */
#define RAW_CHAR_COLOR(raw) (((raw) >> 8) & 0xFF)

/**
 * Extract the character code from a raw character
 */
#define RAW_CHAR_CHR(raw) ((raw) & 0xFF)

/**
 * Used to extract the background color from a packed color value
 */
#define BG_COLOR(color) ((color >> 4) & 15)

/**
 * Used to extract the foreground color from a packed color value
 */
#define FG_COLOR(color) (color & 15)

/**
 * Used to extract the width from a packed size value
 */
#define CONSOLE_WIDTH(size) (size & 0xFFFF)

/**
 * Used to extract the height from a packed size value
 */
#define CONSOLE_HEIGHT(size) ((size >> 16) & 0xFFFF)

/**
 * Used to extract the X position from a packed position value
 */
#define CONSOLE_POSX(pos) (pos & 0xFFFF)

/**
 * Used to extract the Y position from a packed position value
 */
#define CONSOLE_POSY(pos) ((pos >> 16) & 0xFFFF)

#ifdef __KERNEL__

	#include <hardware/context.h>

	void consoleInit();
	void consoleClear();

	void consoleSetColor(uint8_t color);
	uint8_t consoleGetColor();
	uint8_t consoleGetColorForeground();
	uint8_t consoleGetColorBackground();

	void consolePutChar(char chr);
	void consoleEchoChar(char chr);
	void consolePutCharRaw(uint16_t chr);
	void consoleWriteString(const char *str);
	void consoleWriteStringLen(const char *str, size_t len);
	void consoleWriteRawLen(const uint16_t *data, size_t count);
	void consoleWriteStringMax(const char *str, size_t len);
	void consoleWriteHex32(uint32_t value);
	void consoleWriteInt32(uint32_t value);
	void consoleWriteHex16(uint16_t value);

	bool consoleSetCursorPos(uint8_t x, uint8_t y);
	uint32_t consoleGetCursorPos();
	uint32_t consoleGetCursorPosX();
	uint32_t consoleGetCursorPosY();

	void consoleScrollUp();

	uint32_t consoleGetSize();
	uint8_t consoleGetHeight();
	uint8_t consoleGetWidth();

	void consoleShowHardwareCursor(bool changed);
	void consoleSetHardwareCursor(uint8_t x, uint8_t y);
	uint32_t consoleGetHardwareCursor();

	void consoleSetFont();
	void consoleSetFlags(uint32_t newFlags);
	uint32_t consoleGetFlags();

	void consoleSystemFailure(const char **lines, uint32_t numArgs, uint32_t *args, struct taskContext *context);

#endif
/**
 * @}
 */
#endif /* _H_CONSOLE_ */