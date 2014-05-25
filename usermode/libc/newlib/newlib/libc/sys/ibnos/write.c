#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <stdint.h>
#include <reent.h>
#include <errno.h>
#include "syscall.h"

_ssize_t _write_r(struct _reent *eptr, int file, const void *ptr, size_t len)
{
	int32_t ret;
	size_t written 	= 0;
	eptr->_errno 	= 0;

	while (len > 0)
	{
		/* write some data (if possible) */
		ret = objectWrite(file, ptr, len);
		if (ret < 0)
			break;

		len 	-= ret;
		written	+= ret;

		/* we couldn't write any data, let's wait till there is some space in the kernel buffer */
		if (len && ret == 0)
			objectWait(file, 1);
	}

	if (ret < 0)
	{
		eptr->_errno = EPIPE;
		return -1;
	}
	return written;
}
