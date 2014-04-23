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

#ifndef _H_PIC_
#define _H_PIC_

#ifdef __KERNEL__

	#include <stdbool.h>
	#include <stdint.h>

	#define PIC1_PORT			0x20
	#define PIC2_PORT			0xA0
	#define PIC1_COMMAND_PORT	PIC1_PORT
	#define PIC1_DATA_PORT		(PIC1_PORT+1)
	#define PIC2_COMMAND_PORT	PIC2_PORT
	#define PIC2_DATA_PORT		(PIC2_PORT+1)

	#define ICW1_ICW4			0x01
	#define ICW1_SINGLE			0x02
	#define ICW1_INTERVAL4		0x04
	#define ICW1_LEVEL			0x08
	#define ICW1_INIT			0x10

	#define ICW4_8086			0x01
	#define ICW4_AUTO			0x02
	#define ICW4_BUF_SLAVE		0x08
	#define ICW4_BUF_MASTER		0x0C
	#define ICW4_SFNM			0x10

	#define PIC_EOI				0x20

	#define IRQ_COUNT			16

	/**
	 * \addtogroup PIC
	 * @{
	 * @name IRQ Ports
	 * @{
	 */
	#define IRQ_PIT			0 /**< \ref PIT								*/
	#define IRQ_KEYBOARD	1 /**< A key was pressed on the Keyboard	*/
	#define IRQ_SLAVE		2 /**< The slave PIC received an IRQ		*/
	#define IRQ_COM2		3 /**< The COM Port 2 received data			*/
	#define IRQ_COM1		4 /**< The COM Port 1 received data			*/
	#define IRQ_LPT2		5 /**< The LPT Port 2 received data			*/
	#define IRQ_FLOPPY		6 /**< The floppy driver has read some data	*/
	#define IRQ_LPT1		7 /**< The LPT Port 1 received data			*/
	#define IRQ_CMOS_CLOCK	8 /**< The CMOS clock was just updated		*/
	#define IRQ_PS2_MOUSE	12 /**< The mouse was moved or clicked		*/
	#define IRQ_FPU			13 /**< Not used in modern CPUs				*/
	#define IRQ_ATA1		14 /**< First ATA device (Hard disk or CD)	*/
	#define IRQ_ATA2		15 /**< Second ATA device (Hard disk or CD)	*/
	/**
	 * @}
	 * @}
	 */
	typedef uint32_t (*irq_callback)(uint32_t irq);

	void picInit(uint32_t interruptOffset);
	bool picReserveIRQ(uint32_t irq, irq_callback callback);
	void picFreeIRQ(uint32_t irq);

#endif

#endif /* _H_PIC_ */