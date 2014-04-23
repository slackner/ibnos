#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>
#include <stdint.h>
#include "syscall.h"

_ssize_t _read_r(struct _reent *eptr, int file, void *ptr, size_t len)
{
	int32_t ret 	= 0;
	eptr->_errno 	= 0;

	while (1)
	{
		/* read some data (if possible) */
		ret = ibnos_syscall(SYSCALL_OBJECT_READ, (uint32_t)file, (uint32_t)ptr, (uint32_t)len);

		/* did we get some data or an error ? */
		if (ret != 0)
			break;

		/* wait till there is some data */
		ibnos_syscall(SYSCALL_OBJECT_WAIT, (uint32_t)file, 0);
	}

	if (ret < 0)
		eptr->_errno = EPIPE;

	return ret;
}
