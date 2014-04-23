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

#include <process/semaphore.h>
#include <process/object.h>
#include <memory/allocator.h>
#include <util/list.h>
#include <util/util.h>

/** \addtogroup Semaphores
 *  @{
 *	Implementation of Semaphores
 */

static void __semaphoreDestroy(struct object *obj);
static void __semaphoreShutdown(struct object *obj, UNUSED uint32_t mode);
static int32_t __semaphoreGetStatus(struct object *obj, UNUSED uint32_t mode);
static struct linkedList *__semaphoreWait(struct object *obj, UNUSED uint32_t mode, uint32_t *result);
static void __semaphoreSignal(struct object *obj, UNUSED uint32_t result);

static const struct objectFunctions semaphoreFunctions =
{
	__semaphoreDestroy,
	NULL, /* getMinHandle */
	__semaphoreShutdown,
	__semaphoreGetStatus,
	__semaphoreWait,
	__semaphoreSignal,
	NULL, /* write */
	NULL, /* read */
	NULL, /* insert */
	NULL, /* remove */
};

/**
 * @brief Creates a new kernel semaphore object
 * @details This function allocates and returns the memory for a new kernel semaphore
 *			object, which can for example be used for synchronization between multiple
 *			threads.
 *
 * @param value Initial value of the semaphore object
 * @return Pointer to the new kernel semaphore object
 */
struct semaphore *semaphoreCreate(uint32_t value)
{
	struct semaphore *s;

	/* allocate some new memory */
	if (!(s = heapAlloc(sizeof(*s))))
		return NULL;

	/* initialize general object info */
	__objectInit(&s->obj, &semaphoreFunctions);
	ll_init(&s->waiters);
	s->value = value;

	return s;
}

/**
 * @brief Destructor for kernel semaphore objects
 *
 * @param obj Pointer to the kernel semaphore object
 */
static void __semaphoreDestroy(struct object *obj)
{
	struct semaphore *s = objectContainer(obj, struct semaphore, &semaphoreFunctions);

	/* release other threads if the user destroys the object before they return */
	queueWakeup(&s->waiters, true, -1);
	assert(ll_empty(&s->waiters));

	/* release semaphore memory */
	s->obj.functions = NULL;
	heapFree(s);
}

/**
 * @brief Aborts all pending wait operations
 * @details This function allows to abort all pending wait operations on the given
 *			kernel semaphore object. All waiting threads will return (-1), similar as if
 *			the object was destroyed.
 *
 * @param obj Pointer to the kernel semaphore object
 * @param mode not used
 */
static void __semaphoreShutdown(struct object *obj, UNUSED uint32_t mode)
{
	struct semaphore *s = objectContainer(obj, struct semaphore, &semaphoreFunctions);
	queueWakeup(&s->waiters, true, -1);
}

/**
 * @brief Queries the current value of the kernel semaphore object
 * @details This function returns the current value of the kernel semaphore object.
 *			It is not safe to rely on the result of this function in a multithreading
 *			environment where other threads could modify the counter in the meantime.
 *
 * @param obj Pointer to the kernel semaphore object
 * @param mode not used
 *
 * @return Current counter of the semaphore object
 */
static int32_t __semaphoreGetStatus(struct object *obj, UNUSED uint32_t mode)
{
	struct semaphore *s = objectContainer(obj, struct semaphore, &semaphoreFunctions);
	return s->value;
}

/**
 * @brief Performs a semaphore wait operation
 * @details This function will try to decrement the counter of the semaphore object.
 *			If this is possible the function will return immediately, whereas the result
 *			represents the new counter of the semaphore. It is not safe to rely on the
 *			result of this function in a multithreading environment, where other threads
 *			could also modify the counter in the meantime. If the counter is already zero
 *			and cannot be decremented, the operation is blocking till another thread
 *			increments it with a semaphore signal operation.
 *
 * @param obj Pointer to the kernel semaphore object
 * @param mode not used
 * @param result Will be filled out with the result code if the operation doesn't block
 * @return NULL if the operation returns immediately, otherwise a pointer to a wait queue linked list
 */
static struct linkedList *__semaphoreWait(struct object *obj, UNUSED uint32_t mode, uint32_t *result)
{
	struct semaphore *s = objectContainer(obj, struct semaphore, &semaphoreFunctions);
	if (s->value == 0) return &s->waiters;

	*result = --s->value;
	return NULL;
}

/**
 * @brief Performs a semaphore signal operation
 * @details If there are no threads waiting for the semaphore this function will
 *			increment the semaphore counter by one. Otherwise one of the waiting
 *			threads will wake up with a result value of 0, as the semaphore counter
 *			hasn't changed its value.
 *
 * @param obj Pointer to the kernel semaphore object
 * @param result not used
 */
static void __semaphoreSignal(struct object *obj, UNUSED uint32_t result)
{
	struct semaphore *s = objectContainer(obj, struct semaphore, &semaphoreFunctions);
	if (ll_empty(&s->waiters))
		s->value++;
	else
	{
		assert(s->value == 0);
		queueWakeup(&s->waiters, false, 0);
	}
}

/**
 * @}
 */
