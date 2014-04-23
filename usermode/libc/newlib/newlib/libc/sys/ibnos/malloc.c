#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <unistd.h>
#include <errno.h>
#include <reent.h>
#include "syscall.h"

#define _ALLOC_SKIP_DEFINE

#include "liballoc.h"

volatile int memory_lock = 0;

int liballoc_lock()
{
    while (__sync_lock_test_and_set(&memory_lock, 1)) {};
    return 0;
}

int liballoc_unlock()
{
    __sync_synchronize(); // Memory barrier.
    memory_lock = 0;
    return 0;
}

void* liballoc_alloc(int size)
{
	return (void*)ibnos_syscall(SYSCALL_ALLOCATE_MEMORY, size);
}

int liballoc_free(void *ptr, int size)
{
	return ibnos_syscall(SYSCALL_RELEASE_MEMORY, (uint32_t)ptr, size);
}
/* 
	The *_r are functions required for internal commands. Since we do
	not use the default allocator of newlib we need to implement them
*/ 
void* _malloc_r (struct _reent *ptr, size_t size)
{
	void *addr = liballoc_malloc_r(size);
	if (!addr && ptr)
		ptr->_errno = ENOMEM;
	return addr;
}

void* _realloc_r (struct _reent *eptr, void* ptr, size_t size)
{
	void *addr = liballoc_realloc_r(ptr, size);
	if (!addr && eptr)
		eptr->_errno = ENOMEM;
	return addr;
}

void* _calloc_r (struct _reent *ptr, size_t num, size_t size)
{
	void *addr = liballoc_calloc_r(num, size);
	if (!addr && ptr)
		ptr->_errno = ENOMEM;
	return addr;
}

void _free_r (struct _reent *eptr, void* ptr)
{
	liballoc_free_r(ptr);
}

/* Just forward to the internal functions */
void* malloc (size_t size)
{
	return _malloc_r(__getreent(), size);
}

void* realloc (void* ptr, size_t size)
{
	return _realloc_r(__getreent(), ptr, size);
}

void* calloc (size_t num, size_t size)
{
	return _calloc_r(__getreent(), num, size);
}

void free (void* ptr)
{
	_free_r(__getreent(), ptr);
}