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

#include <memory/physmem.h>
#include <memory/paging.h>
#include <util/util.h>
#include <util/list.h>

/** \addtogroup Allocator
 *  @{
 *  Implementation of the kernel memory allocator.
 */


#define HEAP_ALIGN_SIZE 16
#define HEAP_ALIGN_MASK (HEAP_ALIGN_SIZE - 1)
#define SMALL_HEAP_MAGIC 0xFEEFABB1
#define LARGE_HEAP_MAGIC 0xFEEFABB2

struct heapEntry
{
	uint32_t heapMagic;
	struct linkedList entry;
	uint32_t length		: 31;
	uint32_t reserved	: 1;
};

/*
 * Small block heap
 */

static struct linkedList smallHeap				= LL_INIT(smallHeap);

static struct linkedList smallUnusedHeap32		= LL_INIT(smallUnusedHeap32);
static struct linkedList smallUnusedHeap64		= LL_INIT(smallUnusedHeap64);
static struct linkedList smallUnusedHeap128		= LL_INIT(smallUnusedHeap128);
static struct linkedList smallUnusedHeap256		= LL_INIT(smallUnusedHeap256);
static struct linkedList smallUnusedHeap512		= LL_INIT(smallUnusedHeap512);
static struct linkedList smallUnusedHeap1024	= LL_INIT(smallUnusedHeap1024);

static inline struct heapEntry *__smallFindUnusedHeapEntry(uint32_t length)
{
	struct linkedList *list;

	if (length <= 32		&& !ll_empty(&smallUnusedHeap32))
		list = &smallUnusedHeap32;
	else if (length <= 64	&& !ll_empty(&smallUnusedHeap64))
		list = &smallUnusedHeap64;
	else if (length <= 128	&& !ll_empty(&smallUnusedHeap128))
		list = &smallUnusedHeap128;
	else if (length <= 256	&& !ll_empty(&smallUnusedHeap256))
		list = &smallUnusedHeap256;
	else if (length <= 512	&& !ll_empty(&smallUnusedHeap512))
		list = &smallUnusedHeap512;
	else if (length <= 1024 && !ll_empty(&smallUnusedHeap1024))
		list = &smallUnusedHeap1024;
	else
		return NULL;

	/* return first entry of the corresponding list */
	return LL_ENTRY(ll_remove(list->next), struct heapEntry, entry);
}

static inline struct heapEntry *__smallQueueUnusedHeap(struct heapEntry *heap, uint32_t length)
{
	struct linkedList *list;

	if (((uint32_t)heap & PAGE_MASK) == 0 && length == PAGE_SIZE)
	{
		/* unmap and free this page */
		pagingReleasePhysMem(NULL, heap, 1);
		return NULL;
	}

	if (length >= 1024)
		list = &smallUnusedHeap1024;
	else if (length >= 512)
		list = &smallUnusedHeap512;
	else if (length >= 256)
		list = &smallUnusedHeap256;
	else if (length >= 128)
		list = &smallUnusedHeap128;
	else if (length >= 64)
		list = &smallUnusedHeap64;
	else if (length >= 32)
		list = &smallUnusedHeap32;
	else
		assert(0);

	/* otherwise remember this frame */
	heap->heapMagic	= SMALL_HEAP_MAGIC;
	ll_add_after(list, &heap->entry);
	heap->length	= length;
	heap->reserved	= 0;

	return heap;
}

static inline struct heapEntry *__smallGetPreviousHeap(struct heapEntry *heap)
{
	struct heapEntry *prev_heap = (struct heapEntry *)((uint32_t)heap & ~PAGE_MASK);

	while (prev_heap < heap)
	{
		assert(prev_heap->heapMagic == SMALL_HEAP_MAGIC);
		assert((prev_heap->length & HEAP_ALIGN_MASK) == 0);

		if ((uint32_t)prev_heap + prev_heap->length >= (uint32_t)heap)
			return prev_heap;

		prev_heap = (struct heapEntry *)((uint32_t)prev_heap + prev_heap->length);
	}

	/* at this point the prev_heap should match heap, otherwise the structure is corrupted */
	assert(prev_heap == heap);
	return prev_heap;
}

static inline struct heapEntry *__smallGetNextHeap(struct heapEntry *heap, uint32_t length)
{
	struct heapEntry *next_heap = (struct heapEntry *)((uint32_t)heap + length);
	if ((uint32_t)next_heap >= ((uint32_t)heap & ~PAGE_MASK) + PAGE_SIZE) return NULL;

	assert(next_heap->heapMagic == SMALL_HEAP_MAGIC);
	assert((next_heap->length & HEAP_ALIGN_MASK) == 0);

	return next_heap;
}

/* internally used to free memory */
static struct heapEntry *__smallInternalFree(struct heapEntry *deleted_heap, uint32_t length)
{
	struct heapEntry *heap;

	assert(((uint32_t)deleted_heap & HEAP_ALIGN_MASK) == 0);
	assert((length & HEAP_ALIGN_MASK) == 0 && length > 0);

	/* step 1: try to merge with previous free areas */
	heap = __smallGetPreviousHeap(deleted_heap);
	if (heap && !heap->reserved)
	{
		ll_remove(&heap->entry);
		length = (uint32_t)deleted_heap + length - (uint32_t)heap;
		deleted_heap = heap;
	}

	/* step 2: merge with consecutive areas */
	heap = __smallGetNextHeap(deleted_heap, length);
	if (heap && !heap->reserved)
	{
		ll_remove(&heap->entry);
		length = (uint32_t)heap + heap->length - (uint32_t)deleted_heap;
	}

	return __smallQueueUnusedHeap(deleted_heap, length);
}

static struct heapEntry *__smallAlloc(uint32_t length)
{
	struct heapEntry *heap;
	uint32_t origHeapLength;

	/* try to find a free area which has the minimum required size */
	length += sizeof(struct heapEntry);
	length = (length + HEAP_ALIGN_MASK) & ~HEAP_ALIGN_MASK;
	heap = __smallFindUnusedHeapEntry(length);

	/* we found a block */
	if (heap)
	{
		/* ensure that the heap is not corrupted */
		assert(heap->heapMagic == SMALL_HEAP_MAGIC);
		assert((heap->length & HEAP_ALIGN_MASK) == 0);
		assert(heap->length >= length);
		assert(!heap->reserved);
		origHeapLength = heap->length;
	}
	else
	{
		heap = pagingAllocatePhysMem(NULL, 1, true, false);
		origHeapLength = PAGE_SIZE;
	}

	/* use full memory if the remaining space is too small to be useful */
	if (origHeapLength < length + sizeof(struct heapEntry) + 16)
		length = origHeapLength;

	heap->heapMagic	= SMALL_HEAP_MAGIC;
	ll_add_after(&smallHeap, &heap->entry);
	heap->length	= length;
	heap->reserved	= 1;

	/* give the rest back to the owner */
	if (length < origHeapLength)
		__smallInternalFree((struct heapEntry *)((uint32_t)heap + length), origHeapLength - length);

	return heap;
}

static void __smallFree(struct heapEntry *heap)
{
	assert(heap->heapMagic == SMALL_HEAP_MAGIC);
	assert((heap->length & HEAP_ALIGN_MASK) == 0);
	assert(heap->reserved);

	ll_remove(&heap->entry);
	__smallInternalFree(heap, heap->length);
}

static struct heapEntry *__smallReAlloc(struct heapEntry *heap, uint32_t length)
{
	struct heapEntry *next_heap;
	uint32_t origHeapLength;

	assert(heap->heapMagic == SMALL_HEAP_MAGIC);
	assert((heap->length & HEAP_ALIGN_MASK) == 0);
	assert(heap->reserved);

	/* it makes more sense to store this in a large heap entry */
	if (length > 1024 - sizeof(struct heapEntry))
		return NULL;

	length += sizeof(struct heapEntry);
	length = (length + HEAP_ALIGN_MASK) & ~HEAP_ALIGN_MASK;

	origHeapLength = heap->length;
	if (length > origHeapLength)
	{
		/* necessary to increase size, take a look at the next entry afterwards */
		next_heap = __smallGetNextHeap(heap, heap->length);
		if (!next_heap || next_heap->reserved) return NULL;
		if (length > (uint32_t)next_heap + next_heap->length - (uint32_t)heap) return NULL;

		/* temporarily let our current block span both of them */
		ll_remove(&next_heap->entry);
		origHeapLength = (uint32_t)next_heap + next_heap->length - (uint32_t)heap;
	}

	/* at this point the total buffer should be sufficient */
	assert(origHeapLength >= length);

	/* use full memory if the remaining space is too small to be useful */
	if (origHeapLength < length + sizeof(struct heapEntry) + 16)
		length = origHeapLength;

	/* element is still in the smallList */
	heap->heapMagic	= SMALL_HEAP_MAGIC;
	heap->length	= length;
	heap->reserved	= 1;

	/* give the rest back to the owner */
	if (length < origHeapLength)
		__smallInternalFree((struct heapEntry *)((uint32_t)heap + length), origHeapLength - length);

	return heap;
}

/*
 * Large block heap
 */

static struct linkedList largeHeap = LL_INIT(largeHeap);

static struct heapEntry *__largeAlloc(uint32_t length)
{
	struct heapEntry *heap;

	/* add overhead for entry and round up to next page boundary */
	length += sizeof(struct heapEntry);
	length = (length + PAGE_MASK) & ~PAGE_MASK;
	heap = pagingAllocatePhysMem(NULL, length >> PAGE_BITS, true, false);

	/* pur into our vector of allocated blocks */
	heap->heapMagic = LARGE_HEAP_MAGIC;
	ll_add_after(&largeHeap, &heap->entry);
	heap->length	= length;
	heap->reserved	= 1;

	return heap;
}

static void __largeFree(struct heapEntry *heap)
{
	/* uint32_t length; */

	assert(((uint32_t)heap & PAGE_MASK) == 0);
	assert(heap->heapMagic == LARGE_HEAP_MAGIC);
	assert((heap->length & PAGE_MASK) == 0);
	assert(heap->reserved);

	ll_remove(&heap->entry);

	pagingReleasePhysMem(NULL, heap, heap->length >> PAGE_BITS);
}

static struct heapEntry *__largeReAlloc(struct heapEntry *heap, uint32_t length)
{
	assert(((uint32_t)heap & PAGE_MASK) == 0);
	assert(heap->heapMagic == LARGE_HEAP_MAGIC);
	assert((heap->length & PAGE_MASK) == 0);

	/* it makes more sense to store this in a small heap entry */
	if (length <= 1024 - sizeof(struct heapEntry))
		return NULL;

	/* add overhead for entry and round up to next page boundary */
	length += sizeof(struct heapEntry);
	length = (length + PAGE_MASK) & ~PAGE_MASK;

	/* fast-path */
	if (length == heap->length)
		return heap;

	/* temporarily remove the entry while we're relocating */
	ll_remove(&heap->entry);

	/* let the paging layer find some better spot for this */
	heap = pagingReAllocatePhysMem(NULL, heap, heap->length >> PAGE_BITS, length >> PAGE_BITS, true, false);

	/* update all the values */
	assert(heap->heapMagic == LARGE_HEAP_MAGIC);
	ll_add_after(&largeHeap, &heap->entry);
	heap->length	= length;
	heap->reserved	= 1;

	return heap;
}

/**
 * @brief Allocates a block of kernel memory
 * @details Allocates a block of kernel memory of the requested size. The algorithm
 *			will always return a pointer aligned to a 16-byte boundary, or NULL
 *			if the request cannot be fulfilled. The new memory block will not be
 *			initialized.
 *
 * @param length Number of bytes to allocate
 * @return Pointer to the allocated memory block or NULL if the request cannot be fulfilled
 */
void *heapAlloc(uint32_t length)
{
	struct heapEntry *heap;
	if (!length) return NULL;

	if (length <= 1024 - sizeof(struct heapEntry))
		heap = __smallAlloc(length);
	else
		heap = __largeAlloc(length);

	return heap ? (void *)((uint32_t)heap + sizeof(struct heapEntry)) : NULL;
}

/**
 * @brief Deallocates a block of kernel memory
 * @details Verifies that the provided memory pointer is valid, and afterwards
 *			releases the full block. If something goes wrong an assertion is
 *			thrown. This typically means that the application has corrupted the
 *			block header.
 *
 * @param addr Pointer to a memory block allocated with heapAlloc()
 */
void heapFree(void *addr)
{
	struct heapEntry *heap;
	if (!addr) return;

	assert(((uint32_t)addr & PAGE_MASK) >= sizeof(struct heapEntry));

	heap = (struct heapEntry *)((uint32_t)addr - sizeof(struct heapEntry));
	assert(heap->heapMagic == SMALL_HEAP_MAGIC || heap->heapMagic == LARGE_HEAP_MAGIC);
	assert(heap->reserved);

	if (heap->heapMagic == SMALL_HEAP_MAGIC)
		__smallFree(heap);
	else
		__largeFree(heap);
}

/**
 * @brief Determines the size of a specific kernel memory block
 * @details Returns the number of bytes of a specific kernel memory block. The
 *			result of this function can be greater than the requested size
 *			specified when calling heapAlloc() due to align issues.
 *
 * @param addr Pointer to a memory block allocated with heapAlloc()
 * @return Size of the memory block in bytes
 */
uint32_t heapSize(void *addr)
{
	struct heapEntry *heap;
	if (!addr) return 0;

	assert(((uint32_t)addr & PAGE_MASK) >= sizeof(struct heapEntry));

	heap = (struct heapEntry *)((uint32_t)addr - sizeof(struct heapEntry));
	assert(heap->heapMagic == SMALL_HEAP_MAGIC || heap->heapMagic == LARGE_HEAP_MAGIC);
	assert(heap->reserved);

	return heap->length - sizeof(struct heapEntry);
}

/**
 * @brief Resizes a block of kernel memory
 * @details Provides a fast method to change the size of a memory block. If the
 *			return value is NULL then it was impossible to fulfill the request
 *			(for example not enough memory left). Otherwise this function returns
 *			a pointer to the new location of the memory block, which can be, but
 *			is not necessarily equal to the previous location. The new bytes of
 *			memory are not initialized before this function returns.
 *
 * @param addr Pointer to a memory block
 * @param length New requested length
 *
 * @return Pointer to a new block of kernel memory
 */
void *heapReAlloc(void *addr, uint32_t length)
{
	struct heapEntry *heap, *new_heap = NULL;
	void *new_addr;

	/* no addr given -> allocate memory, no length -> free memory */
	if (!addr)
		return heapAlloc(length);

	if (!length)
	{
		heapFree(addr);
		return NULL;
	}

	assert(((uint32_t)addr & PAGE_MASK) >= sizeof(struct heapEntry));

	heap = (struct heapEntry *)((uint32_t)addr - sizeof(struct heapEntry));
	assert(heap->heapMagic == SMALL_HEAP_MAGIC || heap->heapMagic == LARGE_HEAP_MAGIC);
	assert(heap->reserved);

	if (heap->heapMagic == SMALL_HEAP_MAGIC)
		new_heap = __smallReAlloc(heap, length);
	else
		new_heap = __largeReAlloc(heap, length);

	/* return the new heap if we had success */
	if (new_heap)
		return (void *)((uint32_t)new_heap + sizeof(struct heapEntry));

	/* no faster method, we allocate a new block and copy the data */
	new_addr = heapAlloc(length);
	if (!new_addr) return NULL;

	/* copy data */
	if (length > (heap->length - sizeof(struct heapEntry)))
		length = (heap->length - sizeof(struct heapEntry));
	memcpy(new_addr, addr, length);

	heapFree(addr);
	return new_addr;
}

/**
 * @brief Runs some internal checks to ensure that the heap is still valid
 */
void heapVerify()
{
	struct heapEntry *heap;

	LL_FOR_EACH(heap, &smallHeap, struct heapEntry, entry)
	{
		assert(((uint32_t)heap & HEAP_ALIGN_MASK) == 0);
		assert(heap->heapMagic == SMALL_HEAP_MAGIC);
		assert(heap->length >= sizeof(struct heapEntry) + 16);
		assert(heap->reserved);
	}

	LL_FOR_EACH(heap, &largeHeap, struct heapEntry, entry)
	{
		assert(((uint32_t)heap & PAGE_MASK) == 0);
		assert(heap->heapMagic == LARGE_HEAP_MAGIC);
		assert(heap->length > 0 && (heap->length & PAGE_MASK) == 0);
		assert(heap->reserved);
	}

	#define VALIDATE_UNUSED_LIST(unused_list) \
		do \
		{ \
			LL_FOR_EACH(heap, unused_list, struct heapEntry, entry) \
			{ \
				assert(((uint32_t)heap & HEAP_ALIGN_MASK) == 0); \
				assert(heap->heapMagic == SMALL_HEAP_MAGIC); \
				assert(heap->length >= sizeof(struct heapEntry) + 16); \
				assert(!heap->reserved); \
			} \
		} \
		while(0)

	VALIDATE_UNUSED_LIST(&smallUnusedHeap32);
	VALIDATE_UNUSED_LIST(&smallUnusedHeap64);
	VALIDATE_UNUSED_LIST(&smallUnusedHeap128);
	VALIDATE_UNUSED_LIST(&smallUnusedHeap256);
	VALIDATE_UNUSED_LIST(&smallUnusedHeap512);
	VALIDATE_UNUSED_LIST(&smallUnusedHeap1024);

	#undef VALIDATE_UNUSED_LIST
}

/**
 * @}
 */