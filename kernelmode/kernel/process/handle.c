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

#include <process/handle.h>
#include <memory/allocator.h>
#include <process/object.h>
#include <util/list.h>
#include <util/util.h>

/** \addtogroup Handles
 *  @{
 *  These functions are used to manage handles opened by a process.
 */


/**
 * @brief Initializes the handle table structure which is part of each process
 * @details Initializes all members of the handle table structure, which is used to
 *			keep track of all kernel objects available for the user mode. The newly
 *			allocated table is empty.
 *
 * @param table Pointer to the handle table
 */
void handleTableInit(struct handleTable *table)
{
	table->count		= MIN_HANDLES;
	table->free_begin	= 0; /* first free element */
	table->free_end		= 0; /* last used element + 1 */
	table->handles		= heapAlloc(table->count * sizeof(struct object *));

	assert(table->handles);
	memset(table->handles, 0, table->count * sizeof(struct object *));
}

/**
 * @brief Forks a handle table.
 * @details Initializes the destination handle table and duplicates references to
 *			all the kernel objects in the source handle table. This function is used
 *			to implement the fork syscall.
 *
 * @param destination Pointer to the destination handle table (which will be initialized)
 * @param source Pointer to the source handle table
 */
void handleForkTable(struct handleTable *destination, struct handleTable *source)
{
	uint32_t i;
	uint32_t count = MIN_HANDLES;
	while (count < source->free_end) count *= 2;

	assert(source->handles);
	assert(count >= source->free_end);

	destination->count			= count;
	destination->free_begin		= source->free_begin;
	destination->free_end		= source->free_end;
	destination->handles		= heapAlloc(count * sizeof(struct object *));

	assert(destination->handles);
	memset(destination->handles, 0, destination->count * sizeof(struct object *));

	for (i = 0; i < source->free_end; i++)
		destination->handles[i] = source->handles[i] ? __objectAddRef(source->handles[i]) : NULL;
}

/**
 * @brief Releases the memory of the handle table and all associated objects
 *
 * @param table Pointer to the handle table
 */
void handleTableFree(struct handleTable *table)
{
	uint32_t i;

	for (i = 0; i < table->free_end; i++)
	{
		if (table->handles[i]) __objectRelease(table->handles[i]);
	}

	heapFree(table->handles);
}

/**
 * @brief Allocates a handle (index) for a specific kernel object
 * @details Allocates the lowest possible handle for a specific kernel object. This
 *			function also increases the refcount, such that the object will stay there
 *			until the last reference was deleted.
 *
 * @param table Pointer to the handle table
 * @param object Pointer to some kernel object
 *
 * @return Handle which is valid inside of the process associated to the table
 */
uint32_t handleAllocate(struct handleTable *table, struct object *object)
{
	uint32_t i;
	assert(object);

	/* start iteration with the lowest possible handle */
	i  = __objectGetMinHandle(object);
	if (i < table->free_begin) i = table->free_begin;

	for (; i < table->free_end; i++)
	{
		if (!table->handles[i])
		{
			table->free_begin = i + 1;
			table->handles[i] = __objectAddRef(object);
			return i;
		}
	}

	if (i >= table->count)
	{
		uint32_t count = 2 * table->count;
		while (count < i) count *= 2;
		if (count > MAX_HANDLES) count = MAX_HANDLES;

		/* unable to allocate further handles */
		if (table->count >= count) return -1;

		table->handles = heapReAlloc(table->handles, count * sizeof(struct object *));
		memset(table->handles + table->count, 0, (count - table->count) * sizeof(struct object *));
		table->count   = count;

		assert(table->handles);
	}

	table->free_begin = i + 1;
	table->free_end   = i + 1;
	table->handles[i] = __objectAddRef(object);
	return i;
}

/**
 * @brief Associates a handle (index) with a kernel object
 * @details Releases the previous object which was associated with the handle
 *			(if any), afterwards assigns the new kernel object to it.
 *
 * @param table Pointer to the handle table
 * @param handle Handle which will be replaced
 * @param object Pointer to some kernel object
 * @return True on success, otherwise false (handle out of range)
 */
bool handleSet(struct handleTable *table, uint32_t handle, struct object *object)
{
	struct object *old_object;
	assert(object);

	if (handle >= MAX_HANDLES) return false;

	if (handle >= table->count)
	{
		uint32_t count = 2 * table->count;
		while (count < handle) count *= 2;
		if (count > MAX_HANDLES) count = MAX_HANDLES;
		assert(count > table->count);

		table->handles = heapReAlloc(table->handles, count * sizeof(struct object *));
		memset(table->handles + table->count, 0, (count - table->count) * sizeof(struct object *));
		table->count   = count;

		assert(table->handles);
	}

	/* replace object */
	old_object = table->handles[handle];
	table->handles[handle] = __objectAddRef(object);

	/* update first free if necessary */
	if (handle == table->free_begin)
		table->free_begin = handle + 1;

	/* update (last used + 1) if necessary */
	if (handle >= table->free_end)
		table->free_end = handle + 1;

	if (old_object) __objectRelease(old_object);
	return true;
}

/**
 * @brief Returns the kernel object associated to a handle
 *
 * @param table Pointer to the handle table
 * @param handle Handle / index
 *
 * @return Pointer to the kernel object (without increasing refcount)
 */
struct object *handleGet(struct handleTable *table, uint32_t handle)
{
	if (handle >= table->free_end) return NULL;
	return table->handles[handle];
}

/**
 * @brief Releases the object associated with a handle
 *
 * @param table Pointer to the handle table
 * @param handle Handle / index which will be released
 *
 * @return True on success, otherwise false (out of range or not set)
 */
bool handleRelease(struct handleTable *table, uint32_t handle)
{
	struct object *object;

	if (handle >= table->free_end) return false;
	if (!(object = table->handles[handle])) return false;
	table->handles[handle] = NULL;

	/* update first free if necessary */
	if (handle < table->free_begin)
		table->free_begin = handle;

	/* update (last used + 1) if necessary */
	if (handle == table->free_end - 1)
	{
		/* decrease table->free_last till we hit the last used handle */
		while (table->free_end > 0 && !table->handles[table->free_end - 1])
			table->free_end--;

		if (table->free_end > MIN_HANDLES && table->free_end < table->count / 4)
		{
			uint32_t count = table->count / 4;
			if (count < MIN_HANDLES) count = MIN_HANDLES;

			table->handles = heapReAlloc(table->handles, count * sizeof(struct object *));
			table->count   = count;

			assert(table->handles);
		}
	}

	__objectRelease(object);
	return true;
}

/**
 * @brief Returns the number of handles in a handletable
 *
 * @param table Pointer to the handle table
 * @return Number of handles
 */
uint32_t handleCount(struct handleTable *table)
{
	uint32_t i, count = 0;
	for (i = 0; i < table->free_end; i++)
	{
		if (table->handles[i]) count++;
	}
	return count;
}
/**
 * @}
 */