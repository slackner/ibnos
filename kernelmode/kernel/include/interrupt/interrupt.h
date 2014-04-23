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
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS DDOR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _H_INTERRUPT_
#define _H_INTERRUPT_


#ifdef __KERNEL__

	#include <process/thread.h>

	/** \addtogroup Interrupts
	 *  @{
	 */

	/**
	 * @anchor InterruptReturnValue
	 * @name Interrupt return values
	 * @{
	 */
	/** The interrupt could not be handled -> system failure / abort process */
	#define INTERRUPT_UNHANDLED						0
	/** The interrupt was handled, continue execution */
	#define INTERRUPT_CONTINUE_EXECUTION			1
	/** Continue with a different task */
	#define INTERRUPT_YIELD							2
	/** Exit the current thread */
	#define INTERRUPT_EXIT_THREAD					3
	/** Exit the current process */
	#define INTERRUPT_EXIT_PROCESS					4
	/**
	 * @}
	 */

	typedef uint32_t (*interrupt_callback)(uint32_t interrupt, uint32_t error, struct thread *t);
	uint32_t dispatchInterrupt(uint32_t interrupt, uint32_t error, struct thread *t);
	bool interruptReserve(uint32_t interrupt, interrupt_callback callback);
	void interruptFree(uint32_t interrupt);

	/**
	 *  @}
	 */
#endif

#endif /* _H_INTERRUPT_ */