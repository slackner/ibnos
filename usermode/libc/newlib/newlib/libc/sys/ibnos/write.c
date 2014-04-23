#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <stdint.h>
#include <reent.h>
#include <errno.h>
#include "syscall.h"

_ssize_t _write_r(struct _reent *eptr, int file, const void *ptr, size_t len)
{
	int32_t ret 	= 0;
	size_t written 	= 0;
	eptr->_errno 	= 0;

	while (len > 0)
	{
		/* write some data (if possible) */
		ret = ibnos_syscall(SYSCALL_OBJECT_WRITE, (uint32_t)file, (uint32_t)ptr, (uint32_t)len);
		if (ret < 0)
			break;

		len 	-= ret;
		written	+= ret;

		/* we couldn't write any data, let's wait till there is some space in the kernel buffer */
		if (len && ret == 0)
			ibnos_syscall(SYSCALL_OBJECT_WAIT, (uint32_t)file, 1);
	}

	if (ret < 0)
	{
		/* there was some error with the pipe */
		eptr->_errno = EPIPE;
		return -1;
	}
	return written;
}
