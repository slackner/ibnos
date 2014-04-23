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

#include <hardware/pit.h>
#include <hardware/pic.h>
#include <util/util.h>
#include <io/io.h>

/**
 * \defgroup PIT Programmable Intervall Timer
 * \addtogroup PIT
 *  @{
 *  The programmable Intervall Timer is a chip which allows controlling
 *  3 different timers. The first timer can produce a hardware IRQ
 *  and is mainly used for things like task scheduling. The second timer
 *  is not used anymore on modern systems and the third timer is used
 *  to control the PC Speaker frequency.
 *
 *  For more information take a look at http://wiki.osdev.org/Programmable_Interval_Timer
 */

/**
 * @brief Internal function to set a value of the PIT
 *
 * @param channel Either 0, 1 or 2
 * @param mode One of the \ref TimerModes "Timer modes"
 * @param value The value to set
 */
void pitSetValue(uint32_t channel, uint32_t mode, uint16_t value)
{
	assert(channel < PIT_CHANNEL_COUNT);

	outb(PIT_MODE_PORT, PIT_CONTROL_VALUE(PIT_BINARY, mode, PIT_LSB_MSB, channel));
	outb(PIT_CHANNEL_BASE + channel, value & 0xFF);
	outb(PIT_CHANNEL_BASE + channel, value >> 8);
}

/**
 * @brief Set frequency of the programmable interval timer
 * @details This function allows setting the timer on one of the three timer
 *			channels of the PIT.
 *
 * @warning The frequency must be between 19 Hz and 1193182 Hz
 *
 * @param channel Either 0, 1 or 2
 * @param frequency The frequency to set
 */
void pitSetFrequency(uint32_t channel, uint32_t frequency)
{
	uint32_t value;
	assert(channel < PIT_CHANNEL_COUNT);
	/*
	 * There are only a few values for which this assert would be true,
	 * so I commented it out since it is better to have a slilightly different
	 * frequency instead of being stuck with some unusable frequency. The only
	 * usable exception would be 82 Hz = ~12 ms.
	 */
	/* assert((PIT_FREQUENCY % frequency) == 0); */
	value = PIT_FREQUENCY / frequency;
	assert(value);

	pitSetValue(channel, PIT_MODE_RATE_GENERATOR, value);
}
/** @}*/