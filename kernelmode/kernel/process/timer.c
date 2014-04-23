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

#include <process/timer.h>
#include <process/object.h>
#include <memory/allocator.h>
#include <interrupt/interrupt.h>
#include <hardware/pic.h>
#include <hardware/pit.h>
#include <util/list.h>
#include <util/util.h>

/** \addtogroup Timer
 *  @{
 *  Implementation of timer functions.
 */

#define TIMER_INTERRUPT_FREQUENCY	82 /* Hz */
#define TIMER_INTERRUPT_DELTA		12 /* ms */

static uint64_t currentKernelTimestamp;

struct linkedList timerList = LL_INIT(timerList);

static void __timerDestroy(struct object *obj);
static void __timerShutdown(struct object *obj, UNUSED uint32_t mode);
static int32_t __timerGetStatus(struct object *obj, UNUSED uint32_t mode);
static struct linkedList *__timerWait(struct object *obj, UNUSED uint32_t mode, uint32_t *result);
static void __timerSignal(struct object *obj, UNUSED uint32_t result);
static int32_t __timerWrite(struct object *obj, uint8_t *buf, uint32_t length);

static const struct objectFunctions timerFunctions =
{
	__timerDestroy,
	NULL, /* getMinHandle */
	__timerShutdown,
	__timerGetStatus,
	__timerWait,
	__timerSignal,
	__timerWrite,
	NULL, /* read */
	NULL, /* insert */
	NULL, /* remove */
};

/* disables the timer in the global timerList */
static inline void __timerDeactivate(struct timer *t)
{
	if (t->active)
	{
		ll_remove(&t->obj.entry);
		t->active = false;
	}
}

/* enables the timer in the global timerList */
static inline void __timerActivate(struct timer *t)
{
	if (!t->active)
	{
		struct timer *cur_t;
		LL_FOR_EACH(cur_t, &timerList, struct timer, obj.entry)
		{
			if (cur_t->timeout > t->timeout) break;
		}
		ll_add_before(&cur_t->obj.entry, &t->obj.entry);
		t->active = true;
	}
}

/* updates the timer in the global timerList */
static inline void __timerUpdate(struct timer *t)
{
	__timerDeactivate(t);
	__timerActivate(t);
}

/**
 * @brief Creates a new kernel timer object
 * @details This function allocates and returns the memory for a new kernel timer
 *			object, which expires after some time or periodically triggers an
 *			event.
 *
 * @param wakeupAll Should all waiting threads wake up on expire?
 * @return Pointer to the new kernel timer object
 */
struct timer *timerCreate(bool wakeupAll)
{
	struct timer *t;

	/* allocate some new memory */
	if (!(t = heapAlloc(sizeof(*t))))
		return NULL;

	/* initialize general object info */
	__objectInit(&t->obj, &timerFunctions);
	ll_init(&t->waiters);
	t->active		= false;
	t->timeout		= 0;
	t->interval		= 0;
	t->wakeupAll	= wakeupAll;

	return t;
}

/**
 * @brief Destructor for kernel timer objects
 *
 * @param obj Pointer to the kernel timer object
 */
static void __timerDestroy(struct object *obj)
{
	struct timer *t = objectContainer(obj, struct timer, &timerFunctions);

	/* deactivate the timer and let all waiting objects return */
	__timerDeactivate(t);
	queueWakeup(&t->waiters, true, -1);
	assert(ll_empty(&t->waiters));

	/* release timer memory */
	t->obj.functions = NULL;
	heapFree(t);
}

/**
 * @brief Aborts all pending wait operations
 * @details This function allows to abort all pending wait operations on the given
 *			kernel timer object. All waiting threads will return (-1), similar as if
 *			the object was destroyed.
 *
 * @param obj Pointer to the kernel timer object
 * @param mode not used
 */
static void __timerShutdown(struct object *obj, UNUSED uint32_t mode)
{
	struct timer *t = objectContainer(obj, struct timer, &timerFunctions);

	/* deactivate the timer and let all waiting objects return */
	__timerDeactivate(t);
	queueWakeup(&t->waiters, true, -1);
}

/**
 * @brief Queries the current state of the kernel timer object
 * @details If the timer is in autoRepeat mode, then this function returns (-1).
 *			Otherwise it will return a boolean value representing if the object
 *			was triggered.
 *
 * @param obj Pointer to the kernel timer object
 * @param mode not used
 *
 * @return True (triggered), false (not yet triggered), or (-1) if in autorepeat mode
 */
static int32_t __timerGetStatus(struct object *obj, UNUSED uint32_t mode)
{
	struct timer *t = objectContainer(obj, struct timer, &timerFunctions);
	if (t->interval) return -1;
	return !t->active;
}

/**
 * @brief Performs a timer wait operation
 * @details This function will wait until the timer is triggered. If the timer is in
 *			autorepeat mode it will automatically be resetted.
 *
 * @param obj Pointer to the kernel timer object
 * @param mode not used
 * @param result Will be filled out with the result (number of events which occured) if the operation doesn't block
 * @return NULL if the operation returns immediately, otherwise a pointer to a wait queue linked list
 */
static struct linkedList *__timerWait(struct object *obj, UNUSED uint32_t mode, uint32_t *result)
{
	struct timer *t = objectContainer(obj, struct timer, &timerFunctions);
	uint32_t eventCount;

	/* wait for event */
	if (t->timeout > currentKernelTimestamp)
	{
		__timerActivate(t);
		return &t->waiters;
	}

	/* Calculate the number of occurances since the timeout elapsed the last time */
	if (t->interval)
	{
		eventCount = (currentKernelTimestamp - t->timeout) / t->interval + 1;
		t->timeout += eventCount * t->interval;
		__timerUpdate(t);
	}
	else
	{
		eventCount = t->active;
		__timerDeactivate(t);
	}

	/* Return number of occurances */
	*result = eventCount;
	return NULL;
}

/**
 * @brief Wakes up one/all objects waiting on the timer
 * @details If there are no threads waiting for the timer this function will
 *			increment the timer counter by one. Otherwise one of the waiting
 *			threads will wake up with a result value of 0, as the timer counter
 *			hasn't changed its value.
 *
 * @param obj Pointer to the kernel timer object
 * @param result not used
 */
static void __timerSignal(struct object *obj, UNUSED uint32_t result)
{
	struct timer *t = objectContainer(obj, struct timer, &timerFunctions);
	queueWakeup(&t->waiters, t->wakeupAll, 0);
}

/**
 * @brief Changes the timeout and interval of a kernel timer
 * @details Buffer should point to a timerInfo structure containing the timeout
 *			and interval. The timer internal information is updated afterwards.
 *
 * @param obj Pointer to the kernel timer object
 * @param buf Pointer to a timerInfo structure
 * @param length Should be sizeof(struct timerInfo)
 * @return (-1) if the structure was invalid, otherwise sizeof(struct timerInfo)
 */
static int32_t __timerWrite(struct object *obj, uint8_t *buf, uint32_t length)
{
	struct timer *t = objectContainer(obj, struct timer, &timerFunctions);
	struct timerInfo *info = (struct timerInfo *)buf;
	if (length != sizeof(struct timerInfo)) return -1;

	/* update timer structure */
	t->timeout  = currentKernelTimestamp + info->timeout;
	t->interval = info->interval;

	/* update the timer */
	__timerUpdate(t);
	return length;
}

/**
 * @brief Handles the timer IRQ
 * @details This function triggers any expired timers and afterwards schedules
 *			the next thread (Round Robin scheduling).
 *
 * @param irq not used
 * @return Always returns #INTERRUPT_YIELD
 */
static uint32_t timer_irq(UNUSED uint32_t irq)
{
	struct timer *t;
	uint32_t eventCount;

	currentKernelTimestamp += TIMER_INTERRUPT_DELTA;

	for (t = LL_ENTRY(timerList.next, struct timer, obj.entry); &t->obj.entry != &timerList;)
	{
		if (t->timeout > currentKernelTimestamp) break;

		/* Calculate the number of occurances since the timeout elapsed the last time */
		if (t->interval)
		{
			eventCount = (currentKernelTimestamp - t->timeout) / t->interval + 1;
			t->timeout += eventCount * t->interval;
			__timerUpdate(t);
		}
		else
		{
			eventCount = 1;
			__timerDeactivate(t);
		}

		/* wake up waiters */
		queueWakeup(&t->waiters, t->wakeupAll, eventCount);

		/* get next timer */
		t = LL_ENTRY(timerList.next, struct timer, obj.entry);
	}

	/* continue with the next thread */
	return INTERRUPT_YIELD;
}

/**
 * @brief Initializes the system timer
 * @details This function initializes the system timer which is used to schedule threads.
 */
void timerInit()
{
	pitSetFrequency(0, TIMER_INTERRUPT_FREQUENCY);
	picReserveIRQ(IRQ_PIT, timer_irq);
}

/**
 * @brief Returns the current kernel timestamp
 * @details This function returns the current kernel timestamp, which represents
 *			the number of milliseconds since the system was booted.
 * @return Kernel timestmap
 */
uint64_t timerGetTimestamp()
{
	return currentKernelTimestamp;
}

/**
 * @}
 */
