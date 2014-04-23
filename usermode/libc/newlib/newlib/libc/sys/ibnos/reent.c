#include <_ansi.h>
#include <reent.h>
#include "syscall.h"

struct _reent * __getreent()
{
	return (struct _reent *)ibnos_syscall(SYSCALL_GET_THREADLOCAL_STORAGE_BASE);
}
