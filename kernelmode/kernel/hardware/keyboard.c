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

#include <hardware/keyboard.h>
#include <hardware/pic.h>
#include <interrupt/interrupt.h>
#include <process/object.h>
#include <console/console.h>
#include <util/util.h>
#include <io/io.h>

/**
 * \addtogroup Keyboard
 *  @{
 *
 *  They Keyboard is controlled through the keyboard controller and emits an IRQ
 *  as soon as a key is pressed.
 */

uint32_t keyboardLEDFlags = 0;
/**
 * Maps the multibyte scancodes to an internal keycode
 */
uint8_t standardKeyCodes[128] =
{
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
	60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 84, 00, 00, 86, 87, 88, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00
};

/**
 * Maps the multi byte scan codes to an internal keycode
 */
uint8_t extendedKeyCodes[128] =
{
	 00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,
	 00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  96,  97,  00,  00,
	 00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,
	 00,  00,  00,  00,  00,  99,  00,  00, 100,  00,  00,  00,  00,  00,  00,  00,
	 00,  00,  00,  00,  00,  00,  00, 102, 103, 104,  00, 105,  00, 106,  00, 107,
	108, 109, 110, 111,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,
	 00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,
	 00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00,  00
};

static struct object *keyboardStdin;

int keyMapIndex = 0;
extern const struct keyMap keyMapDE;

/**
 * @brief Supported keymaps
 */
const struct keyMap *keyMaps =
{
	&keyMapDE
};

struct keyModifier modifiers = {0, 0, 0, 0, 0};

/**
 * @brief Handles key presses after the scancode was decoded
 * @details This function translates the scancode into keycode and afterwards
 *			to characters depending on the loaded keyboard layout. Moreover
 *			it will track the state of modifier keys like capslock or
 *			shift.
 *
 * @param extended Is this a standard or 1 / 2 byte extended scancode?
 * @param scanCode The value of the scancode
 * @param keyUp Is the key pressed or released?
 */
void processScancode(int extended, uint8_t scanCode, bool keyUp)
{
	uint8_t mappedCode = 0;
	uint8_t charCode;
	uint16_t *decodeMap = NULL;

	struct keyEvent keyEv;
	keyEv.pressed	= !keyUp;
	keyEv.keyCode	= 0;
	keyEv.modifiers = modifiers;

	if (extended == 0)
		mappedCode = standardKeyCodes[scanCode];
	else if (extended == 1)
		mappedCode = extendedKeyCodes[scanCode];

	if (!mappedCode)
		return;

	/* TODO: solve this better ?*/
	if (modifiers.shift || modifiers.shiftLocked)
	{
		if (modifiers.ctrl)
			decodeMap = keyMaps[keyMapIndex].keyMaps.shift_ctrl_map;
		else if(modifiers.alt)
			decodeMap = keyMaps[keyMapIndex].keyMaps.shift_alt_map;
		else
			decodeMap = keyMaps[keyMapIndex].keyMaps.shift_map;
	}
	else if (modifiers.ctrl)
	{
		if(modifiers.altgr)
			decodeMap = keyMaps[keyMapIndex].keyMaps.altgr_ctrl_map;
		else
			decodeMap = keyMaps[keyMapIndex].keyMaps.ctrl_map;
	}
	else if (modifiers.altgr)
	{
		if (modifiers.alt)
			decodeMap = keyMaps[keyMapIndex].keyMaps.altgr_alt_map;
		else
			decodeMap = keyMaps[keyMapIndex].keyMaps.altgr_map;
	}
	else if (modifiers.alt)
	{
		decodeMap = keyMaps[keyMapIndex].keyMaps.alt_map;
	}
	else
	{
		decodeMap = keyMaps[keyMapIndex].keyMaps.plain_map;
	}
	keyEv.keyCode = decodeMap[mappedCode] & 0x0FFF;

	if (!keyEv.keyCode)
		return;

	/* TODO: pass the keyEv struct to applications to allow reaction on raw input */

	/* set modifieres */
	if (KEY_TYPE(keyEv.keyCode) == KEY_TYPE_SHIFT)
	{
		switch (KEY_VALUE(keyEv.keyCode))
		{
			case KEY_MODIFIER_SHIFT:
			case KEY_MODIFIER_SHIFTL:
			case KEY_MODIFIER_SHIFTR:
				keyEv.modifiers.shift = keyEv.pressed;
				break;

			case KEY_MODIFIER_CTRL:
			case KEY_MODIFIER_CTRLL:
			case KEY_MODIFIER_CTRLR:
				keyEv.modifiers.ctrl = keyEv.pressed;
				break;

			case KEY_MODIFIER_ALT:
				keyEv.modifiers.alt = keyEv.pressed;
				break;

			case KEY_MODIFIER_ALTGR:
				keyEv.modifiers.altgr = keyEv.pressed;
				break;

			default:
				break;
		}
	}
	else if (KEY_TYPE(keyEv.keyCode) == KEY_TYPE_SPEC)
	{
		if (keyEv.keyCode == KEY_CODE_CAPS && keyEv.pressed)
		{
			keyEv.modifiers.shiftLocked = !keyEv.modifiers.shiftLocked;
		}
	}

	modifiers = keyEv.modifiers;

	if (!keyEv.pressed)
		return;

	if (KEY_TYPE(keyEv.keyCode) == KEY_TYPE_LATIN  || KEY_TYPE(keyEv.keyCode) == KEY_TYPE_ASCII ||
		KEY_TYPE(keyEv.keyCode) == KEY_TYPE_LETTER || keyEv.keyCode == KEY_CODE_ENTER)
	{
		if (keyEv.keyCode == KEY_CODE_ENTER)
			charCode = '\n';
		else
			charCode = KEY_VALUE(keyEv.keyCode);

		consoleEchoChar(charCode);

		if (keyboardStdin)
			__objectWrite(keyboardStdin, &charCode, sizeof(charCode));
	}
	return;
}

static bool ext1Code = 0;
static int ext2Code = 0;
static uint16_t e1_prev = 0;

/**
 * @brief Handles the keyboard IRQ
 * @details This function reads the scan code from the keyboard buffer
 *			and decodes the scancode. The decoded characters are appended
 *			to the stdn pipe of the init process afterwards.
 *
 * @param irq not used
 * @return Always returns #INTERRUPT_CONTINUE_EXECUTION
 */
static uint32_t keyboard_irq(UNUSED uint32_t irq)
{
	uint8_t scanCode = 0;
	bool keyUp = false;

	//while (inb(KEYBOARD_STATUS_PORT) & 0x1)
	//{
		scanCode = inb(KEYBOARD_BUFFER_PORT);

		/* check if this is a key up event */
		if ((scanCode & 0x80) && (ext2Code || (scanCode != 0xE1)) && (ext1Code || (scanCode != 0xE0)))
		{
			scanCode &= ~0x80;
			keyUp = true;
		}

		if (ext1Code)
		{
			/* check for fake shifts */
			if ((scanCode == 0x2A) || (scanCode == 0x36)) {
				ext1Code = false;
				return INTERRUPT_CONTINUE_EXECUTION;
			}
			processScancode(1, scanCode, keyUp);
			ext1Code = false;
		}
		else if (ext2Code == 2)
		{
			e1_prev |= ((uint16_t) scanCode << 8);
			processScancode(2, e1_prev, keyUp);
			ext2Code = 0;
		}
		else if (ext2Code == 1)
		{
			e1_prev = scanCode;
			ext2Code++;
		}
		else if (scanCode == 0xE0)
			ext1Code = true;
		else if (scanCode == 0xE1)
			ext2Code = 1;
		else
			processScancode(0, scanCode, keyUp);
	//}

	return INTERRUPT_CONTINUE_EXECUTION;
}

/**
 * @brief Initializes the keyboard
 * @details This function initializes the keyboard controller and requests
 *			the keyboard IRQ.
 */
void keyboardInit(struct object *obj)
{
	keyboardStdin = obj;

	/* must be set as first to prevent race conditions when clearing the buffer */
	picReserveIRQ(IRQ_KEYBOARD, keyboard_irq);

	/* clear buffer */
	while (inb(KEYBOARD_STATUS_PORT) & 0x1)
		inb(KEYBOARD_BUFFER_PORT);

	/* enable keyboard */
	keyboardSend(KEYBOARD_CMD_ENABLE);

	/* Disable all LEDs */
	keyboardSetLEDFlags(0);
}

/**
 * @brief Send a command to the keyboard controller
 * @details This function will wait till the keyboard input buffer is empty
 *			and then send the command to the controller.
 *
 * @param cmd The command you want to send
 */
void keyboardSend(uint8_t cmd)
{
	/* Wait till the command buffer is empty */
	while ((inb(KEYBOARD_STATUS_PORT) & 0x2))
	{
		/* do nothing */
	}
	outb(KEYBOARD_BUFFER_PORT, cmd);
}
/**
 * @brief Set LED Flags
 * @details Take a look at the \ref keyboardLED "keyboard LED flags" for more information.
 *
 * @param flags The flags to set
 */
void keyboardSetLEDFlags(uint32_t flags)
{
	keyboardLEDFlags = flags & 7;
	keyboardSend(KEYBOARD_CMD_LED);
	keyboardSend(keyboardLEDFlags);
}

/**
 * @brief Return the current active LED flags
 * @return Combination of \ref keyboardLED "keyboard LED flags"
 */
uint32_t keyboardGetLEDFlags()
{
	return keyboardLEDFlags;
}
/** @}*/