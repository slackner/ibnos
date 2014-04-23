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

#include <process/pipe.h>
#include <process/object.h>
#include <memory/allocator.h>
#include <console/console.h>
#include <util/list.h>
#include <util/util.h>

/** \addtogroup Pipes
 *  @{
 *  Implementation of Pipes.
 */


#define MIN_PIPE_BUFFER_SIZE 0x1000
#define MAX_PIPE_BUFFER_SIZE 0x10000

static void __pipeDestroy(struct object *obj);
static uint32_t __pipeGetMinHandle(UNUSED struct object *obj);
static void __pipeShutdown(struct object *obj, uint32_t mode);
static int32_t __pipeGetStatus(struct object *obj, uint32_t mode);
static struct linkedList *__pipeWait(struct object *obj, uint32_t mode, uint32_t *result);
static int32_t __pipeWrite(struct object *obj, uint8_t *buf, uint32_t length);
static int32_t __pipeRead(struct object *obj, uint8_t *buf, uint32_t length);

static const struct objectFunctions pipeFunctions =
{
	__pipeDestroy,
	__pipeGetMinHandle,
	__pipeShutdown,
	__pipeGetStatus,
	__pipeWait,
	NULL, /* signal */
	__pipeWrite,
	__pipeRead,
	NULL, /* insert */
	NULL, /* remove */
};

static void __stdoutDestroy(struct object *obj);
static uint32_t __stdoutGetMinHandle(UNUSED struct object *obj);
static int32_t __stdoutWrite(UNUSED struct object *obj, uint8_t *buf, uint32_t length);

static const struct objectFunctions stdoutFunctions =
{
	__stdoutDestroy,
	__stdoutGetMinHandle,
	NULL, /* getMinHandle */
	NULL, /* shutdown */
	NULL, /* wait */
	NULL, /* signal */
	__stdoutWrite,
	NULL, /* read */
	NULL, /* insert */
	NULL, /* remove */
};

/**
 * @brief Creates a new kernel pipe object
 * @return Pointer to the kernel pipe object
 */
struct pipe *pipeCreate()
{
	struct pipe *p;
	uint8_t *buffer;

	/* allocate some new memory */
	if (!(p = heapAlloc(sizeof(*p))))
		return NULL;

	if (!(buffer = heapAlloc(MIN_PIPE_BUFFER_SIZE)))
	{
		heapFree(p);
		return NULL;
	}

	/* initialize general object info */
	__objectInit(&p->obj, &pipeFunctions);
	ll_init(&p->writeWaiters);
	ll_init(&p->readWaiters);
	p->buffer	= buffer;
	p->size		= MIN_PIPE_BUFFER_SIZE;
	p->writePos = 0;
	p->readPos	= 0;
	p->writeable = true;

	return p;
}

/**
 * @brief Destructor for kernel pipe objects
 *
 * @param obj Pointer to the kernel pipe object
 */
static void __pipeDestroy(struct object *obj)
{
	struct pipe *p = objectContainer(obj, struct pipe, &pipeFunctions);

	/* release other threads if the user destroys the object before they return */
	queueWakeup(&p->writeWaiters, true, -1);
	queueWakeup(&p->readWaiters, true, -1);
	assert(ll_empty(&p->writeWaiters));
	assert(ll_empty(&p->readWaiters));

	/* release buffer, even if it still contains data */
	if (p->buffer) heapFree(p->buffer);

	/* release pipe memory */
	p->obj.functions = NULL;
	heapFree(p);
}

/**
 * @brief Returns the minimum handle number which can be used for pipe objects
 *
 * @param obj Pointer to the kernel pipe object
 * @return Always 0
 */
static uint32_t __pipeGetMinHandle(UNUSED struct object *obj)
{
	/* pipe numbers can start from zero */
	return 0;
}

/**
 * @brief Closes the read or write side of the kernel pipe object
 * @details This function can close the read (0) and/or write (1) side of a kernel pipe
 *			object. As soon as the write side is closed all further attempts to
 *			write something into the pipe will return (-1), but it is still possible
 *			to read the contained data. When the read side is closed, then both read
 *			and write operations will fail in all following calls, and the data in the
 *			buffer is discarded.
 *
 * @param obj Pointer to the kernel pipe object
 * @param mode 1 for the write, 0 for the read
 */
static void __pipeShutdown(struct object *obj, uint32_t mode)
{
	struct pipe *p = objectContainer(obj, struct pipe, &pipeFunctions);

	/* write will be closed anyway */
	p->writeable = false;
	queueWakeup(&p->writeWaiters, true, -1);

	/* close only write mode, and the pipe is not yet empty */
	if (mode && p->writePos != p->readPos) return;

	/* close read mode */
	queueWakeup(&p->readWaiters, true, -1);

	/* release buffer and reset internal structure */
	if (p->buffer) heapFree(p->buffer);
	p->buffer	= NULL;
	p->size		= 0;
	p->writePos = 0;
	p->readPos	= 0;
}

/**
 * @brief Returns the internal status of a kernel pipe object
 * @details When the status of the read-mode (0) is requested, then this command
 *			returns the number of bytes available in the internal buffer. If the
 *			status of the write-mode (1) is queried, it will return the number of
 *			bytes which can be written in a single call without waiting.
 *
 * @param obj Pointer to the kernel pipe object
 * @param mode 1 for the write, 0 for the read
 *
 * @return Number of bytes to read or write without waiting
 */
static int32_t __pipeGetStatus(struct object *obj, uint32_t mode)
{
	struct pipe *p = objectContainer(obj, struct pipe, &pipeFunctions);
	uint32_t used = p->writePos - p->readPos;
	if (!p->writeable && (mode || used == 0)) return -1;
	if (mode) used = MAX_PIPE_BUFFER_SIZE - used;
	return used;
}

/**
 * @brief Wait for the kernel pipe object
 * @details Starts waiting for a kernel pipe object. This function will return back
 *			control immediately if the request is already fulfilled. In read-mode (0)
 *			it will wait for some data to for reading, in write-mode (1) it will wait
 *			for some space for writing data into the pipe.
 *
 * @param obj Pointer to the kernel pipe object
 * @param mode 1 for the write, 0 for the read
 * @param result Will be filled out with the result code if the operation doesn't block
 * @return NULL if the operation returns immediately, otherwise a pointer to a wait queue linked list
 */
static struct linkedList *__pipeWait(struct object *obj, uint32_t mode, uint32_t *result)
{
	struct pipe *p = objectContainer(obj, struct pipe, &pipeFunctions);
	uint32_t used = p->writePos - p->readPos;

	if (mode)
	{
		/* not writeable anymore */
		if (!p->writeable)
		{
			*result = -1;
			return NULL;
		}

		/* writeable, but the whole buffer is used -> wait */
		if (used >= MAX_PIPE_BUFFER_SIZE)
			return &p->writeWaiters;

		/* return number of bytes free in the buffer */
		*result = MAX_PIPE_BUFFER_SIZE - used;
		return NULL;
	}
	else
	{
		/* not writeable, and buffer is empty */
		if (!p->writeable && used == 0)
		{
			*result = -1;
			return NULL;
		}

		/* buffer is empty -> wait */
		if (used == 0)
			return &p->readWaiters;

		/* return number of bytes used in the buffer */
		*result = used;
		return NULL;
	}
}

/**
 * @brief Writes some data into the kernel pipe object
 * @details This function writes a block of data into the kernel pipe object, and
 *			wakes up waiting readers.
 *
 * @param obj Pointer to the kernel pipe object
 * @param buf Pointer to the buffer
 * @param length Number of bytes to write into the pipe
 * @return Number of bytes written
 */
static int32_t __pipeWrite(struct object *obj, uint8_t *buf, uint32_t length)
{
	struct pipe *p = objectContainer(obj, struct pipe, &pipeFunctions);
	uint32_t used = p->writePos - p->readPos;

	/* fail if the pipe isn't writeable anymore */
	if (!p->writeable) return -1;

	/* we expect that the buffer is still there */
	assert(p->buffer);

	/* ensure that buffer size stays below MAX_PIPE_BUFFER_SIZE */
	if (length > MAX_PIPE_BUFFER_SIZE - used)
		length = MAX_PIPE_BUFFER_SIZE - used;

	if (length > p->size - p->writePos)
	{

		/* out of space, move data to the beginning */
		if (p->readPos != 0)
		{
			memmove(p->buffer, p->buffer + p->readPos, p->writePos - p->readPos);
			p->writePos -= p->readPos;
			p->readPos = 0;
		}

		if (length > p->size - p->writePos)
		{
			uint32_t new_size = p->size * 2;
			while (new_size < p->writePos + length) new_size *= 2;

			p->buffer	= heapReAlloc(p->buffer, new_size);
			p->size		= new_size;
			assert(p->buffer);
		}
	}

	if (length > 0)
	{
		memcpy(p->buffer + p->writePos, buf, length);
		p->writePos += length;
		used += length;
	}

	/* wakeup readers if the buffer is not empty anymore */
	if (used) queueWakeup(&p->readWaiters, true, used);

	return length;
}

/**
 * @brief Reads data from a kernel pipe object into a buffer
 * @details This function reads a block of data from a kernel pipe object and
 *			wakes up waiting writers.
 *
 * @param obj Pointer to the kernel pipe object
 * @param buf Pointer to the buffer
 * @param length Number of bytes to read from the pipe
 * @return Number of bytes read
 */
static int32_t __pipeRead(struct object *obj, uint8_t *buf, uint32_t length)
{
	struct pipe *p = objectContainer(obj, struct pipe, &pipeFunctions);
	uint32_t used = p->writePos - p->readPos;

	/* fail if the pipe is empty and nothing can write anymore */
	if (!p->writeable && !used) return -1;

	/* we expect that the buffer is still there */
	assert(p->buffer);

	/* ensure that length stays below used */
	if (length > used) length = used;

	/* read some memory */
	if (length > 0)
	{
		memcpy(buf, p->buffer + p->readPos, length);
		p->readPos += length;
		used -= length;
	}

	/* buffer is now empty */
	if (p->readPos == p->writePos)
	{
		if (!p->writeable)
		{
			heapFree(p->buffer);
			p->buffer	= NULL;
			p->size		= 0;
		}

		p->readPos  = 0;
		p->writePos = 0;
	}

	/* wakeup writers if the buffer is not full anymore */
	if (used < MAX_PIPE_BUFFER_SIZE)
		queueWakeup(&p->writeWaiters, true, p->writeable ? (MAX_PIPE_BUFFER_SIZE - used) : 0);

	return length;
}

/**
 * @brief Creates a new kernel stdout object
 * @details This function allocates a new kernel stdout object, which can be used
 *			to write data into the terminal. It can be seen as a special case of a
 *			pipe object. In contrary to a regular pipe it doesn't support waiting
 *			or buffering.
 * @return Pointer to the kernel stdout object
 */
struct stdout *stdoutCreate()
{
	struct stdout *p;

	/* allocate some new memory */
	if (!(p = heapAlloc(sizeof(*p))))
		return NULL;

	/* initialize general object info */
	__objectInit(&p->obj, &stdoutFunctions);

	return p;
}

/**
 * @brief Destructor for kernel stdout objects
 *
 * @param obj Pointer to the kernel stdout object
 */
static void __stdoutDestroy(struct object *obj)
{
	struct stdout *p = objectContainer(obj, struct stdout, &stdoutFunctions);

	/* release stdout memory */
	p->obj.functions = NULL;
	heapFree(p);
}

/**
 * @brief Returns the minimum handle number which can be used for stdout objects
 *
 * @param obj Pointer to the kernel stdout object
 * @return Always 0
 */
static uint32_t __stdoutGetMinHandle(UNUSED struct object *obj)
{
	/* pipe numbers can start from zero */
	return 0;
}

/**
 * @brief Writes some data directly into the console buffer
 *
 * @param obj not used
 * @param buf Pointer to the buffer
 * @param length Number of bytes to write into the console
 * @return Number of bytes written (always equal to length)
 */
static int32_t __stdoutWrite(UNUSED struct object *obj, uint8_t *buf, uint32_t length)
{
	consoleWriteStringLen((char *)buf, length);
	return length;
}

/**
 * @}
 */
