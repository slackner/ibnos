#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>
#include <stdint.h>
#include "syscall.h"

_ssize_t _read_r(struct _reent *eptr, int file, void *ptr, size_t len)
{
	int32_t ret;

	for (;;)
	{
		/* read some data (if possible) */
		ret = objectRead(file, ptr, len);

		/* did we get some data or an error ? */
		if (ret != 0)
			break;

		/* wait till there is some data */
		objectWait(file, 0);
	}

	eptr->_errno = (ret < 0) ? EPIPE : 0;
	return ret;
}
