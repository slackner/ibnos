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

#ifndef _H_OBJECT_
#define _H_OBJECT_

#ifdef __KERNEL__

	struct object;

	#include <stddef.h>
	#include <stdint.h>
	#include <stdbool.h>

	#include <util/util.h>
	#include <util/list.h>

	struct objectFunctions
	{
		/* destructor */
		void (*destroy)(struct object *obj);
		/* getMinHandle */
		uint32_t (*getMinHandle)(struct object *obj);
		/* shutdown */
		void (*shutdown)(struct object *obj, uint32_t mode);
		/* getStatus */
		int32_t (*getStatus)(struct object *obj, uint32_t mode);
		/* wait */
		struct linkedList* (*wait)(struct object *obj, uint32_t mode, uint32_t *result);
		/* signal */
		void (*signal)(struct object *obj, uint32_t result);
		/* write */
		int32_t (*write)(struct object *obj, uint8_t *buf, uint32_t length);
		/* read */
		int32_t (*read)(struct object *obj, uint8_t *buf, uint32_t length);
		/* attachObj */
		bool (*attachObj)(struct object *obj, struct object *subObj, uint32_t mode, uint32_t ident);
		/* remove */
		bool (*detachObj)(struct object *obj, uint32_t ident);
	};

	#define objectContainer(p, type, functions) \
		((type *)((uint8_t *)__objectCheckType((p), (functions)) - offsetof(type, obj)))

	struct object
	{
		uint32_t refcount;
		const struct objectFunctions *functions;
		struct linkedList entry; /* entry which is used if other applications might wait on it */
	};

	static inline void __objectInit(struct object *obj, const struct objectFunctions *functions)
	{
		obj->refcount	= 1;
		obj->functions	= functions;
		/* no need to initialize obj->entry */
	}

	static inline struct object *__objectCheckType(struct object *obj, const struct objectFunctions *functions)
	{
		assert(obj && obj->functions == functions);
		return obj;
	}

	#define objectAddRef(p) __objectAddRef(&(p)->obj)
	static inline struct object *__objectAddRef(struct object *obj)
	{
		assert(obj);
		obj->refcount++;
		return obj;
	}

	#define objectRelease(p) __objectRelease(&(p)->obj)
	static inline void __objectRelease(struct object *obj)
	{
		assert(obj);
		if (--obj->refcount) return;
		if (obj->functions->destroy) obj->functions->destroy(obj);
	}

	#define objectGetMinHandle(p) __objectGetMinHandle(&(p)->obj)
	static inline uint32_t __objectGetMinHandle(struct object *obj)
	{
		return obj->functions->getMinHandle ? obj->functions->getMinHandle(obj) : 3;
	}

	#define objectShutdown(p, a) __objectShutdown(&(p)->obj, a)
	static inline void __objectShutdown(struct object *obj, uint32_t mode)
	{
		if (obj->functions->shutdown) obj->functions->shutdown(obj, mode);
	}

	#define objectGetStatus(p, a) __objectGetStatus(&(p)->obj, a)
	static inline int32_t __objectGetStatus(struct object *obj, uint32_t mode)
	{
		return obj->functions->getStatus ? obj->functions->getStatus(obj, mode) : (-1);
	}

	#define objectWait(p, a, b) __objectWait(&(p)->obj, a, b)
	static inline struct linkedList *__objectWait(struct object *obj, uint32_t mode, uint32_t *result)
	{
		return obj->functions->wait ? obj->functions->wait(obj, mode, result) : NULL;
	}

	#define objectSignal(p, a) __objectSignal(&(p)->obj, a)
	static inline void __objectSignal(struct object *obj, uint32_t result)
	{
		if (obj->functions->signal) obj->functions->signal(obj, result);
	}

	#define objectWrite(p, a, b) __objectWrite(&(p)->obj, a, b)
	static inline int32_t __objectWrite(struct object *obj, uint8_t *buf, uint32_t length)
	{
		return obj->functions->write ? obj->functions->write(obj, buf, length) : (-1);
	}

	#define objectRead(p, a, b)	__objectRead(&(p)->obj, a, b)
	static inline int32_t __objectRead(struct object *obj, uint8_t *buf, uint32_t length)
	{
		return obj->functions->read ? obj->functions->read(obj, buf, length) : (-1);
	}

	#define objectAttachObj(p, a, b, c) __objectAttachObj(&(p)->obj, a, b, c)
	static inline bool __objectAttachObj(struct object *obj, struct object *subObj, uint32_t mode, uint32_t ident)
	{
		return obj->functions->attachObj ? obj->functions->attachObj(obj, subObj, mode, ident) : false;
	}

	#define objectDetachObj(p, a) __objectDetachObj(&(p)->obj, a)
	static inline bool __objectDetachObj(struct object *obj, uint32_t ident)
	{
		return obj->functions->detachObj ? obj->functions->detachObj(obj, ident) : false;
	}

	static inline void queueWakeup(struct linkedList *queue, bool all, uint32_t eax)
	{
		struct object *obj, *__obj;
		LL_FOR_EACH_SAFE(obj, __obj, queue, struct object, entry)
		{
			__objectSignal(obj, eax);
			if (!all) break;
		}
	}

#endif

#endif /* _H_OBJECT_ */