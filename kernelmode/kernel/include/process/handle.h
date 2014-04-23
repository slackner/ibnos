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

#ifndef _H_HANDLE_
#define _H_HANDLE_

#ifdef __KERNEL__

	#include <stdint.h>
	#include <stdbool.h>

	#include <process/object.h>

	/** \addtogroup Handle
	 *  @{
	 */

	#define MIN_HANDLES 0x100
	#define MAX_HANDLES 0x10000

	struct handleTable
	{
		uint32_t count;
		uint32_t free_begin;
		uint32_t free_end;
		struct object **handles;
	};

	void handleTableInit(struct handleTable *table);
	void handleForkTable(struct handleTable *destination, struct handleTable *source);
	void handleTableFree(struct handleTable *table);

	uint32_t handleAllocate(struct handleTable *table, struct object *object);
	bool handleSet(struct handleTable *table, uint32_t handle, struct object *object);
	struct object *handleGet(struct handleTable *table, uint32_t handle);
	bool handleRelease(struct handleTable *table, uint32_t handle);

	uint32_t handleCount(struct handleTable *table);

	/**
	 *  @}
	 */
#endif

#endif /* _H_HANDLE_ */