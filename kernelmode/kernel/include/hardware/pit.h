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

#ifndef _H_PIT_
#define _H_PIT_

#ifdef __KERNEL__

	#include <stdbool.h>
	#include <stdint.h>

	/**
	 * \addtogroup PIT
	 * @{
	 * @name Timer Modes
	 * @anchor TimerModes
	 * @{
	 */
	#define PIT_MODE_INTERRUPT_ON_TERMINAL	0
	#define PIT_MODE_ONE_SHOT				1
	#define PIT_MODE_RATE_GENERATOR			2
	#define PIT_MODE_SQUARE_WAVE_GENERATOR  3
	#define PIT_MODE_SOFTWARE_STROBE		4
	#define PIT_MODE_HARDWARE_STROBE		5
	/**
	 * @}
	 * @}
	 */

	#define PIT_CHANNEL_BASE	0x40
	#define PIT_CHANNEL0_PORT	(PIT_CHANNEL_BASE)
	#define PIT_CHANNEL1_PORT	(PIT_CHANNEL_BASE+1)
	#define PIT_CHANNEL2_PORT	(PIT_CHANNEL_BASE+2)
	#define PIT_MODE_PORT		0x43

	#define PIT_BINARY 0
	#define PIT_BCD    1

	#define PIT_INTERNAL	0
	#define PIT_LSB			1
	#define PIT_MSB			2
	#define PIT_LSB_MSB		3

	#define PIT_CONTROL_VALUE(FORMAT, MODE, REGISTER, CHANNEL) \
		((CHANNEL) << 6 | (REGISTER) << 4 | (MODE) << 1 | FORMAT)

	#define PIT_CHANNEL_COUNT	3
	#define PIT_FREQUENCY		1193182

	void pitSetValue(uint32_t channel, uint32_t mode, uint16_t value);
	void pitSetFrequency(uint32_t channel, uint32_t frequency);

#endif

#endif /* _H_PIT_ */