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

#include <process/event.h>
#include <process/object.h>
#include <memory/allocator.h>
#include <process/object.h>
#include <util/list.h>
#include <util/util.h>


/** \addtogroup Events
 *  @{
 *	Events are kernel objects which can be used to wait for multiple objects.
 *  They are comparable with epoll on Linux. You can attach other objects like
 *  pipes, semaphores or timers and wait will return as soon as one
 *  of the objects is signalled.
 */

static void __eventDestroy(struct object *obj);
static void __eventShutdown(struct object *obj, UNUSED uint32_t mode);
static int32_t __eventGetStatus(struct object *obj, UNUSED uint32_t mode);
static struct linkedList *__eventWait(struct object *obj, UNUSED uint32_t mode, uint32_t *result);
static void __eventSignal(struct object *obj, uint32_t result);
static bool __eventAttachObj(struct object *obj, struct object *subObj, uint32_t mode, uint32_t ident);
static bool __eventDetachObj(struct object *obj, uint32_t ident);

static const struct objectFunctions eventFunctions =
{
	__eventDestroy,
	NULL, /* getMinHandle */
	__eventShutdown,
	__eventGetStatus,
	__eventWait,
	__eventSignal,
	NULL, /* write */
	NULL, /* read */
	__eventAttachObj,
	__eventDetachObj,
};

static void __subEventSignal(struct object *obj, uint32_t result);

static const struct objectFunctions subEventFunctions =
{
	NULL, /* destroy */
	NULL, /* getMinHandle */
	NULL, /* shutdown */
	NULL, /* getStatus */
	NULL, /* wait */
	__subEventSignal,
	NULL, /* write */
	NULL, /* read */
	NULL, /* insert */
	NULL, /* remove */
};

/**
 * @brief Creates a new kernel event object
 * @details This function creates a new kernel event object and returns a pointer to
 *			it. Event objects can be used for multiple purposes like waiting for an
 *			event which is triggered by a different thread, or to wait on multiple
 *			objects at the same time by associating subobjects to an event.
 *
 * @param wakeupAll If true all waiting objects will wake up as soon as a signal occurs, otherwise only a single one
 * @return Pointer to the kernel event object
 */
struct event *eventCreate(bool wakeupAll)
{
	struct event *e;

	/* allocate some new memory */
	if (!(e = heapAlloc(sizeof(*e))))
		return NULL;

	/* initialize general object info */
	__objectInit(&e->obj, &eventFunctions);
	ll_init(&e->waiters);
	ll_init(&e->subEvents);
	e->status = 0;
	e->wakeupAll = wakeupAll;

	return e;
}

/**
 * @brief Destructor for kernel event objects
 *
 * @param obj Pointer to the kernel event object
 */
static void __eventDestroy(struct object *obj)
{
	struct event *e = objectContainer(obj, struct event, &eventFunctions);
	struct subEvent *sub, *__sub;

	/* release other threads if the user destroys the object before they return */
	queueWakeup(&e->waiters, true, -1);
	assert(ll_empty(&e->waiters));

	LL_FOR_EACH_SAFE(sub, __sub, &e->subEvents, struct subEvent, entry_event)
	{
		if (sub->blocked)
			ll_remove(&sub->obj.entry);
		ll_remove(&sub->entry_event); /* not really necessary, as the memory is deallocated afterwards */
		__objectRelease(sub->wait);

		sub->obj.functions = NULL;
		heapFree(sub);
	}

	/* release event memory */
	e->obj.functions = NULL;
	heapFree(e);
}

/**
 * @brief Abort all pending wait operations
 * @details This function allows to abort all pending wait operations on the given
 *			kernel event object. All waiting threads will return (-1), similar as if
 *			the object was destroyed.
 *
 * @param obj Pointer to the kernel event object
 * @param mode not used
 */
static void __eventShutdown(struct object *obj, UNUSED uint32_t mode)
{
	struct event *e = objectContainer(obj, struct event, &eventFunctions);
	queueWakeup(&e->waiters, true, -1);
}

/**
 * @brief Returns the last status code returned from an internal wait operation
 * @details This command can be used to retrieve the original status code from a
 *			wait operation. Please note that it is not safe to use this command in
 *			an environment with multiple threads, which could have changed the status
 *			code in the meantime.
 *
 * @param obj Pointer to the kernel event object
 * @param mode not used
 *
 * @return Last status code
 */
static int32_t __eventGetStatus(struct object *obj, UNUSED uint32_t mode)
{
	struct event *e = objectContainer(obj, struct event, &eventFunctions);
	return e->status;
}

/**
 * @brief Wait for the kernel event
 * @details Starts waiting for a kernel event. If one of the associated sub objects
 *			is already triggered or doesn't support the wait operation, then the
 *			control is returned back immediately with the corresponding result value.
 *			Otherwise the waiter linkedlist is returned, such that the thread can add
 *			itself to it.
 *
 * @param obj Pointer to the kernel event object
 * @param mode not used
 * @param result Will be filled out with the result code if the operation doesn't block
 * @return NULL if the operation returns immediately, otherwise a pointer to a wait queue linked list
 */
static struct linkedList *__eventWait(struct object *obj, UNUSED uint32_t mode, uint32_t *result)
{
	struct event *e = objectContainer(obj, struct event, &eventFunctions);
	struct subEvent *sub;
	struct linkedList *queue;

	LL_FOR_EACH(sub, &e->subEvents, struct subEvent, entry_event)
	{
		if (sub->blocked) continue;
		assert(sub->event == e);

		e->status = 0;
		queue = __objectWait(sub->wait, sub->waitMode, &e->status);
		if (queue)
		{
			sub->blocked = true;
			ll_add_after(queue, &sub->obj.entry);
		}
		else
		{
			*result = sub->identifier;
			return NULL;
		}
	}

	return &e->waiters;
}

/**
 * @brief Wakes up one or all threads waiting on the kernel event object
 *
 * @param obj Pointer to the kernel event object
 * @param result Result of the wait operation
 */
static void __eventSignal(struct object *obj, uint32_t result)
{
	struct event *e = objectContainer(obj, struct event, &eventFunctions);
	e->status = 0;
	queueWakeup(&e->waiters, e->wakeupAll, result);
}

/**
 * @brief Associates the kernel event object with a subobject
 * @details This function associates the kernel event object with a subobject. The
 *			event object will internally keep track of all associated subobjects,
 *			and use them for the wait operation. Whenever the wait operation returns
 *			the result will be the corresponding identifier.
 *
 * @param obj Pointer to the kernel event object
 * @param subObj Pointer to some other kernel object (called subobject)
 * @param mode Mode for the wait operation of the subobject
 * @param ident Identifier which will be returned if this object is triggered
 * @return Returns true if the subobject was assigned successfully, otherwise false
 */
static bool __eventAttachObj(struct object *obj, struct object *subObj, uint32_t mode, uint32_t ident)
{
	struct subEvent *sub;
	struct event *e = objectContainer(obj, struct event, &eventFunctions);

	if (!(sub = heapAlloc(sizeof(*sub))))
		return false;

	/* initialize general object info */
	__objectInit(&sub->obj, &subEventFunctions);
	sub->blocked	= false;
	sub->event		= e;
	ll_add_tail(&e->subEvents, &sub->entry_event);
	sub->wait		= __objectAddRef(subObj);
	sub->waitMode	= mode;
	sub->identifier	= ident;

	return true;
}

/**
 * @brief Disassociates the kernel event object from all subjects with a specific identifier
 *
 * @param obj Pointer to the kernel event object
 * @param subObj Pointer to some other kernel object (called subobject)
 * @param mode Mode for the wait operation of the subobject
 * @return Returns true if the subobject was disassociated, otherwise false
 */
static bool __eventDetachObj(struct object *obj, uint32_t ident)
{
	struct event *e = objectContainer(obj, struct event, &eventFunctions);
	struct subEvent *sub, *__sub;
	bool success = false;

	LL_FOR_EACH_SAFE(sub, __sub, &e->subEvents, struct subEvent, entry_event)
	{
		if (sub->identifier == ident)
		{
			if (sub->blocked)
				ll_remove(&sub->obj.entry);
			ll_remove(&sub->entry_event);
			__objectRelease(sub->wait);

			sub->obj.functions = NULL;
			heapFree(sub);

			success = true;
		}
	}

	return success;
}

/**
 * @brief Used internally to implement waiting for multiple objects
 *
 * @param object Pointer to the kernel subEvent object
 * @param result Result of the wait operation
 */
static void __subEventSignal(struct object *obj, uint32_t result)
{
	struct subEvent *s = objectContainer(obj, struct subEvent, &subEventFunctions);
	struct event *e = s->event;
	if (!s->blocked) return;

	s->blocked = false;
	ll_remove(&s->obj.entry);

	/* wakeup objects waiting for the main event */
	e->status = result;
	queueWakeup(&e->waiters, e->wakeupAll, s->identifier);
}

/**
 * @}
 */
