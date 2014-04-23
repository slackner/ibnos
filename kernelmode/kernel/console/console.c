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

#include <console/console.h>
#include <memory/physmem.h>
#include <util/util.h>
#include <io/io.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define CHAR_OFFSET(x, y) ((y) * VGA_WIDTH + (x))

/** In text mode characters are displayed by writing them at specific address in memory */
static uint16_t *videoTextMemory	= (uint16_t *)0xB8000;
static uint8_t  *videoFontMemory	= (uint8_t *)0xA0000;
/** I/O Port used to control the hardware cursor */
static UNUSED uint16_t *videoIOPort = (uint16_t *)0x0463;

/** Current position of the text cursor */
static uint8_t cursorX = 0;
static uint8_t cursorY = 0;

/** Current color of the console */
static uint8_t consoleColor = MAKE_COLOR(COLOR_WHITE, COLOR_BLACK);

/** Flags which control functions like the hardware cursor */
static uint32_t consoleFlags = CONFLAGS_ECHO | CONFLAGS_HWCURSOR | CONFLAGS_HWCURSOR_AUTO;

/** Current position of the hardware cursor */
static uint8_t cursorHardwareX = 0;
static uint8_t cursorHardwareY = 0;

/** Used for integer -> hex conversions */
static const char hexTable[] = "0123456789ABCDEF";

/** Control the console output register (required for hardware cursor) */
static void __consoleSetMiscOutputRegister(bool set)
{
	uint8_t value = inb(0x3CC);

	if (set)
		value |= 1;
	else
		value &= ~1;

	outb(0x3C2, value);
}

/**
 * @brief Initializes the console
 * @details This function claims the memory area of the VGA graphic card to
 *			protect it from being treated as assignable memory. This function
 *			is called during the bootup of the operating system.
 */
void consoleInit()
{
	/* Protect the whole VGA address space */
	physMemProtectBootEntry(0xA0000, 0xC0000 - 0xA0000);
}

/**
 * @brief Clear the screen contents
 * @details This function removes all text from the screen and sets the cursor
 *			to the top left corner.
 */
void consoleClear()
{
	uint32_t x, y;
	for (y = 0; y < VGA_HEIGHT; y++)
	{
		for (x = 0; x < VGA_WIDTH; x++)
			videoTextMemory[CHAR_OFFSET(x, y)] = ' ' | (consoleColor << 8);
	}

	consoleSetCursorPos(0, 0);

	if (consoleFlags & CONFLAGS_HWCURSOR_AUTO)
		consoleSetHardwareCursor(0, 0);
}

/**
 * @brief Set the text and background color of the console
 * @details This function alters the current color of the console and will
 *			affect all further write operations. The color will stay active
 *			till it is changed again.
 *
 * @param color The color to set, use the MAKE_COLOR() macro
 */
void consoleSetColor(uint8_t color)
{
	consoleColor = color;
}

 /**
  * @brief Get current color
  * @details The returned value contains the background and foreground color.
  *			 Use the #BG_COLOR and #FG_COLOR macro to get the individual values.
  * @return Foreground and background color
  */
uint8_t consoleGetColor()
{
	return consoleColor;
}


 /**
  * @brief Get current foreground color
  * @return Foreground color
  */
uint8_t consoleGetColorForeground()
{
	return consoleColor & 15;
}

/**
 * @brief Get current background color
 * @return Background color
 */
uint8_t consoleGetColorBackground()
{
	return (consoleColor >> 4) & 15;
}

/**
 * @brief Write a character on the console
 * @details This functions writes a character on the terminal at the current
 *			cursor position. It will automatically insert line breaks if
 *			necessary (i.e. the end of the line is reached).
 *
 * @param chr The character which should be printed
 */
void consolePutChar(char chr)
{
	bool rawMode = consoleFlags & CONFLAGS_RAW_MODE;
	if (rawMode || chr != '\n')
	{
		videoTextMemory[CHAR_OFFSET(cursorX, cursorY)] = (chr & 0xFF) | (consoleColor << 8);
		cursorX++;
	}

	if ((chr == '\n' && !rawMode) || cursorX >= VGA_WIDTH)
	{
		cursorY++;
		cursorX = 0;
	}

	if (cursorY >= VGA_HEIGHT)
	{
		if (rawMode)
			cursorY = 0;
		else
		{
			consoleScrollUp();
			cursorY = VGA_HEIGHT - 1;
		}
	}

	if (consoleFlags & CONFLAGS_HWCURSOR_AUTO)
		consoleSetHardwareCursor(cursorX, cursorY);
}

/**
 * @brief Echo keyboard input on the console
 * @details This function is used internally by the keyboard driver to directly
 *			print the typed character onto the screen.
 *
 * @param chr Entered characters
 */
void consoleEchoChar(char chr)
{
	if (consoleFlags & CONFLAGS_ECHO)
		consolePutChar(chr);
}

/**
 * @brief Write a raw character and color on the console
 * @details This functions writes a character and color on the terminal
 *			at the current cursor position. The character code and color
 *			is packed in a 16 bit value. Use the #MAKE_RAW_CHAR macro to
 *			create such values.
 *
 * @param chr The character which should be printed
 */
void consolePutCharRaw(uint16_t chr)
{
	bool rawMode = consoleFlags & CONFLAGS_RAW_MODE;
	videoTextMemory[CHAR_OFFSET(cursorX, cursorY)] = chr;
	cursorX++;

	if ((RAW_CHAR_CHR(chr) == '\n' && !rawMode) || cursorX >= VGA_WIDTH)
	{
		cursorY++;
		cursorX = 0;
	}

	if (cursorY >= VGA_HEIGHT)
	{
		if (rawMode)
			cursorY = 0;
		else
		{
			consoleScrollUp();
			cursorY = VGA_HEIGHT - 1;
		}
	}

	if (consoleFlags & CONFLAGS_HWCURSOR_AUTO)
		consoleSetHardwareCursor(cursorX, cursorY);
}

/**
 * @brief Write a C string to the console
 *
 * @param str Pointer to the string
 */
void consoleWriteString(const char *str)
{
	while (*str)
	{
		consolePutChar(*str++);
	}
}

/**
 * @brief Write a string with a fixed size to the console
 *
 * @param str Pointer to the string
 * @param len Number of bytes to write
 */
void consoleWriteStringLen(const char *str, size_t len)
{
	while (len)
	{
		consolePutChar(*str++);
		len--;
	}
}

/**
 * @brief Write raw data with a fixed size to the console
 *
 * @param data Pointer to the data
 * @param count Number of characters to write
 */
void consoleWriteRawLen(const uint16_t *data, size_t count)
{
	while (count)
	{
		consolePutCharRaw(*data++);
		count--;
	}
}

/**
 * @brief Write a nullterminated string or a maximum of len characters
 * @details This function writes a string to the console, but will stop
 *			after either len character have been written or when it detects
 *			a NULL character inside of the string.
 *
 * @param str Pointer to the string
 * @param len Maximum number of bytes to print
 */
void consoleWriteStringMax(const char *str, size_t len)
{
	while (len && *str)
	{
		consolePutChar(*str++);
		len--;
	}
}

/**
 * @brief Write a 32 bit integer as hex value on the console
 * @details This function will convert a 32 bit value to a hex string and print
 *			it at the current cursor position.
 *
 * @param value The value to print
 */
void consoleWriteHex32(uint32_t value)
{
	char buf[8];
	int i;

	for (i = 7; i >= 0; i--)
	{
		buf[i] = hexTable[value & 0xF];
		value >>= 4;
	}

	consoleWriteString("0x");
	consoleWriteStringLen(buf, 8);
}

/**
 * @brief Write a 32 bit integer as decimal value on the console
 * @details This function will convert a 32 bit value to a decimal string and
 *			print it at the current cursor position.
 *
 * @param value The value to print
 */
void consoleWriteInt32(uint32_t value)
{
	char buf[12];
	int i = 11;

	buf[i] = 0;
	do
	{
		buf[--i] = '0' + (value % 10);
		value /= 10;
	}
	while (value);

	consoleWriteString(buf + i);
}

/**
 * @brief Write a 16 bit integer as hex value on the console
 * @details This function will convert a 16 bit value to a hex string and print
 *			it at the current cursor position.
 *
 * @param value The value to print
 */
void consoleWriteHex16(uint16_t value)
{
	char buf[4];
	int i;

	for (i = 3; i >= 0; i--)
	{
		buf[i] = hexTable[value & 0xF];
		value >>= 4;
	}

	consoleWriteString("0x");
	consoleWriteStringLen(buf, 4);
}

/**
 * @brief Set the position of the text cursor
 *
 * @param x X coordinate to set
 * @param y Y coordinate to set
 *
 * @return True if successful or false otherwise (coordinate out of range)
 */
bool consoleSetCursorPos(uint8_t x, uint8_t y)
{
	if (x >= VGA_WIDTH || y >= VGA_HEIGHT)
		return false;

	cursorX = x;
	cursorY = y;

	return true;
}

/**
 * @brief Get packed position of the text cursor
 * @details Use the #CONSOLE_POSX and #CONSOLE_POSY macro to extract the
 *			the X and Y position from this value.
 * @return Packed position containing the X and Y coordinate
 */
uint32_t consoleGetCursorPos()
{
	return (cursorY << 16) | cursorX;
}

/**
 * @brief Get X position of the text cursor
 * @return X coordinate
 */
uint32_t consoleGetCursorPosX()
{
	return cursorX;
}

/**
 * @brief Get Y position of the text cursor
 * @return Y coordinate
 */
uint32_t consoleGetCursorPosY()
{
	return cursorY;
}

/**
 * @brief Scroll the console up one line
 * @details This function scrolls the console up one line, so that every
 *			character is move to the previous line and the first line is
 *			discarded. The cursors are not modified.
 */
void consoleScrollUp()
{
	uint32_t x, y;

	for (y = 1; y < VGA_HEIGHT; y++)
	{
		for (x = 0; x < VGA_WIDTH; x++)
			videoTextMemory[CHAR_OFFSET(x, y-1)] = videoTextMemory[CHAR_OFFSET(x, y)];
	}
	for (x = 0; x < VGA_WIDTH; x++)
		videoTextMemory[CHAR_OFFSET(x, VGA_HEIGHT - 1)] = ' ' | (consoleColor << 8);
}

/**
 * @brief Returns the height (text rows) of the console
 * @return Height
 */
uint8_t consoleGetHeight()
{
	return VGA_HEIGHT;
}

/**
 * @brief Returns the width (text cols) of the console
 * @return Width
 */
uint8_t consoleGetWidth()
{
	return VGA_WIDTH;
}

/**
 * @brief Returns the packed size of the console
 * @details Use the #CONSOLE_HEIGHT and #CONSOLE_WIDTH macros to get the
 *			individual values.
 * @return Packed size
 */
uint32_t consoleGetSize()
{
	return (VGA_HEIGHT << 16) | VGA_WIDTH;
}

/**
 * @brief Show / hide blinking hardware cursor
 *
 * @param changed True if the cursor visibility changed
 */
void consoleShowHardwareCursor(bool changed)
{
	__consoleSetMiscOutputRegister(true);

	if (consoleFlags & CONFLAGS_HWCURSOR)
	{
		/* skip these step if only the position is going to be updated */
		if (changed)
		{
			outb(0x3D4, 0x0A);
			outb(0x3D5, 0x00);
		}

		uint32_t indexPos = CHAR_OFFSET(cursorHardwareX, cursorHardwareY);

		outb(0x3D4, 0x0F);
		outb(0x3D5, indexPos & 0xFF);
		outb(0x3D4, 0x0E);
		outb(0x3D5, (indexPos >> 8) & 0xFF);
	}
	else
	{
		/* see http://www.osdever.net/FreeVGA/vga/crtcreg.htm#0A */
		outb(0x3D4, 0x0A);
		outb(0x3D5, 0x10);
	}
}

/**
 * @brief Set the position of the hardware cursor
 *
 * @param x X coordinate
 * @param y Y coordinate
 */
void consoleSetHardwareCursor(uint8_t x, uint8_t y)
{
	cursorHardwareX = x;
	cursorHardwareY = y;

	if (consoleFlags & CONFLAGS_HWCURSOR)
		consoleShowHardwareCursor(false);
}

/**
 * @brief Get packed position of the hardware cursor
 * @details Use the #CONSOLE_POSX and #CONSOLE_POSY macro to extract the
 *			the X and Y position from this value.
 * @return Packed position containing the X and Y coordinate
 */
uint32_t consoleGetHardwareCursor()
{
	 return (cursorHardwareY << 16) | cursorHardwareX;
}

extern uint8_t vga_latin1[256][16];

/**
 * @brief Load a custom font supporting latin1 characters
 * @details This function loads a custom font (see vgafont.c)
 *			supporting latin1 encoding which is required to display
 *			some characters like the german ü, ä, ö or the € symbol.
 */
void consoleSetFont()
{
	uint32_t i;

	outw(0x03ce, 0x5);
	outw(0x03ce, 0x0406);
	outw(0x03c4, 0x0402);
	outw(0x03c4, 0x0604);

	/*
	 * Characters can have a size up to 8x32. The memory always has the
	 * same size independent of the selected font size. The additional
	 * bytes are simply not used.
	 */
	for (i = 0; i < 256; i++)
		memcpy(videoFontMemory + i * 32, &vga_latin1[i], 16);

	outw(0x03c4, 0x0302);
	outw(0x03c4, 0x0204);
	outw(0x03ce, 0x1005);
	outw(0x03ce, 0x0E06);
}

/**
 * @brief Set console flags
 *
 * @param flags Combination of the \ref CONSOLE_FLAGS values
 */
void consoleSetFlags(uint32_t flags)
{
	consoleFlags = flags;
	consoleShowHardwareCursor(true);
}

/**
 * @brief Get console flags
 * @return Returns the current the active \ref CONSOLE_FLAGS flags
 */
uint32_t consoleGetFlags()
{
	return consoleFlags;
}

/**
 * @brief Print a system failure message and halts the system
 * @details This function displays an error message, some optional hex values
 *			(for example the error code or offending function arguments),
 *			plus the current CPU context. The first line defines the header
 *			of the crash screen and all other lines will be shown as normal
 *			text.
 *
 * @param lines Array of string pointers, terminated with a NULL pointer
 * @param numArgs Number of 32 bit integers to print
 * @param args Array of 32 bit integers
 * @param context The CPU context which should be shown or NULL otherwise
 *
 * @code
 * char *msg[] = {" ERROR TITLE ", "First line", "Second line", NULL};
 * uint32_t arguments[] = {0, 1, 2};
 *
 * consoleSystemFailure(msg, 3, arguments, NULL);
 * @endcode
 */
void consoleSystemFailure(const char **lines, uint32_t numArgs, uint32_t *args, struct taskContext *context)
{
	consoleSetFlags(CONFLAGS_HWCURSOR | CONFLAGS_HWCURSOR_AUTO);
	consoleSetColor(MAKE_COLOR(COLOR_WHITE, COLOR_LIGHT_BLUE));
	consoleClear();

	/* print the title and text lines */
	if (lines)
	{
		if (*lines)
		{
			size_t length = stringLength(*lines);
			consoleSetCursorPos((length < VGA_WIDTH) ? ((VGA_WIDTH - length) / 2) : 0, 2);
			consoleSetColor(MAKE_COLOR(COLOR_LIGHT_BLUE, COLOR_WHITE));
			consoleWriteString(*lines);
			consoleSetColor(MAKE_COLOR(COLOR_WHITE, COLOR_LIGHT_BLUE));
			consoleWriteString("\n\n");
			lines++;
		}

		while (*lines)
			consoleWriteString(*lines++);
		consoleWriteString("\n\n");
	}

	if (numArgs && args)
	{
		uint32_t i;
		for (i = 0; i < numArgs; i++, args++)
		{
			consoleWriteString("  Exception argument ");
			consoleWriteInt32(i);
			consoleWriteString(": ");
			consoleWriteHex32(*args);
			consoleWriteString("\n");
		}
		consoleWriteString("\n");
	}

	if (context)
	{
		consoleWriteString("  Registers:\n");
		consoleWriteString("    cr3 = ");
		consoleWriteHex32(context->cr3);
		consoleWriteString(", eip = ");
		consoleWriteHex32(context->eip);
		consoleWriteString(", efl = ");
		consoleWriteHex32(context->eflags);
		consoleWriteString("\n    eax = ");
		consoleWriteHex32(context->eax);
		consoleWriteString(", ecx = ");
		consoleWriteHex32(context->ecx);
		consoleWriteString(", edx = ");
		consoleWriteHex32(context->edx);
		consoleWriteString(", ebx = ");
		consoleWriteHex32(context->ebx);
		consoleWriteString("\n    esp = ");
		consoleWriteHex32(context->esp);
		consoleWriteString(", ebp = ");
		consoleWriteHex32(context->ebp);
		consoleWriteString(", esi = ");
		consoleWriteHex32(context->esi);
		consoleWriteString(", edi = ");
		consoleWriteHex32(context->edi);
		consoleWriteString("\n\n");

		consoleWriteString("  Segments:\n");
		consoleWriteString("    es  = ");
		consoleWriteHex32(context->es);
		consoleWriteString(", cs  = ");
		consoleWriteHex32(context->cs);
		consoleWriteString(", ss  = ");
		consoleWriteHex32(context->ss);
		consoleWriteString("\n    ds  = ");
		consoleWriteHex32(context->ds);
		consoleWriteString(", fs  = ");
		consoleWriteHex32(context->fs);
		consoleWriteString(", gs  = ");
		consoleWriteHex32(context->gs);
		consoleWriteString("\n\n");
	}

	/* halt processor, this function will never return */
	debugHalt();
}