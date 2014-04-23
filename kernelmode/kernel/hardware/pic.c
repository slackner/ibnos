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

#include <hardware/pic.h>
#include <interrupt/interrupt.h>
#include <console/console.h>
#include <util/util.h>
#include <io/io.h>

/**
 * \defgroup PIC Programmable Interrupt Controller
 * \addtogroup PIC
 *  @{
 *
 *  The Programmable Interrupt Controller (PIC) can be used to control hardware
 *  interrupts / IRQs. A device driver can set an IRQ handler through picReserveIRQ()
 *  and free it again with picFreeIRQ().
 *
 *  For more information about IRQs and Interrupts take a look at http://wiki.osdev.org/IRQ
 */
uint32_t irqBase = 0;

irq_callback irqTable[IRQ_COUNT];

/**
 * @brief Handle an interrupt which is assigned to an IRQ
 * @details This function is executed whenever an interrupt is called which is
 *			assigned to a hardware IRQ. It lookups up the device driver / callback
 *			which is assigned to this IRQ and tells the programmable interrupt
 *			controller that we handled this IRQ, so that we receive further notifications.
 *
 * @param interrupt Used to determine the original IRQ
 * @param error Does not exist for IRQs
 * @param t Not used for IRQs
 * @return One of the \ref InterruptReturnValue "Interrupt return values"
 */
static uint32_t interrupt_irq(uint32_t interrupt, UNUSED uint32_t error, UNUSED struct thread *t)
{
	uint32_t status = INTERRUPT_CONTINUE_EXECUTION;
	uint32_t irq	= interrupt - irqBase;
	assert(irq < IRQ_COUNT);

	/* TODO: handle spurious IRQs */

	/*
	 * We should never get irq 2 since it is only used to notify the master PIC
	 * about irqs on the slave PIC and the slave IRQs have a higher priority.
	 * Since we also send EOI to the master when an IRQ on the slave is
	 * signaled, this interrupt should never reach the CPU.
	 */
	assert(irq != IRQ_SLAVE);

	if (irqTable[irq])
		status = irqTable[irq](irq);
	else
	{
		consoleWriteString("Unhandled IRQ: ");
		consoleWriteHex32(irq);
		consoleWriteString("\n");
	}

	/* Send EOI */
	if(irq >= 8)
		outb(PIC2_COMMAND_PORT, PIC_EOI);

	outb(PIC1_COMMAND_PORT, PIC_EOI);

	return status;
}

/**
 * @brief Initializes the programmable interrupt controller
 * @details The PIC is required to handle hardware interrupts / IRQs.
 *			There are (theoretically) 16 hardware IRQs which can be mapped
 *			to interrupts.
 *
 * @param interruptOffset The first interrupt which should handle the IRQs
 */
void picInit(uint32_t interruptOffset)
{
	uint32_t i;

	/* the offset must be a multiple of 8 */
	assert((interruptOffset & 7) == 0);
	irqBase = interruptOffset;

	/* Initialize in Cascade mode */
	outb(PIC1_COMMAND_PORT, ICW1_INIT+ICW1_ICW4);
	outb(PIC1_COMMAND_PORT, ICW1_INIT+ICW1_ICW4);

	outb(PIC1_DATA_PORT, interruptOffset);
	outb(PIC2_DATA_PORT, interruptOffset + 8);

	outb(PIC1_DATA_PORT, 4);
	outb(PIC2_DATA_PORT, 2);

	outb(PIC1_DATA_PORT, ICW4_8086);
	outb(PIC2_DATA_PORT, ICW4_8086);

	/*
	 *	Disable all IRQs except the slave IRQ, it won't signal until we enable
	 *	an IRQ on the slave chip.
	 */
	outb(PIC1_DATA_PORT, 0xF & (~IRQ_SLAVE));
	outb(PIC2_DATA_PORT, 0xF);

	/* reset the irqTable */
	for (i = 0; i < IRQ_COUNT; i++)
		irqTable[i] = NULL;

	/* reserve all 16 interrupts which are now connected to an IRQ */
	for (i = 0; i < IRQ_COUNT; i++)
		assert(interruptReserve(interruptOffset + i, interrupt_irq));
}

/**
 * @brief Assign a callback function to an IRQ
 * @details This function allows a device driver to get notified on an IRQ
 *			by executing a callback. The function can then handle the input
 *			and return the control back to the user program.
 *
 * @param irq The irq which should be assigned
 * @param callback The function which should be called on the irq
 *
 * @return True, if the interrupt is not taken yet, false otherwise.
 */
bool picReserveIRQ(uint32_t irq, irq_callback callback)
{
	uint16_t port;
	uint8_t mask;

	assert(irq < IRQ_COUNT);
	if (irqTable[irq])
		return false;

	assert(irq != IRQ_SLAVE);
	irqTable[irq] = callback;

	/* actually unmask the irq on the PIC */
	if(irq < 8)
	{
		port = PIC1_DATA_PORT;
	}
	else
	{
		port = PIC2_DATA_PORT;
		irq -= 8;
	}

	mask = inb(port);
	mask &= ~(1 << irq);
	outb(port, mask);

	return true;
}
/**
 * @brief Release an IRQ
 * @details Call this function if you do not longer need to listen for this IRQ.
 *
 * @param irq The previously requested IRQ
 */
void picFreeIRQ(uint32_t irq)
{
	uint16_t port;
	uint8_t mask;

	assert(irq < IRQ_COUNT);
	if (!irqTable[irq])
		return;

	assert(irq != IRQ_SLAVE);
	irqTable[irq] = NULL;

	if(irq < 8)
	{
		port = PIC1_DATA_PORT;
	}
	else
	{
		port = PIC2_DATA_PORT;
		irq -= 8;
	}

	mask = inb(port);
	mask |= 1 << irq;
	outb(port, mask);
}
/** @}*/