#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>
#include "syscall.h"

int _close_r(struct _reent* ptr, int fd)
{
	if (!objectClose(fd))
	{
		ptr->_errno = EBADF;
		return -1;
	}
	else
	{
		ptr->_errno = 0;
		return 0;
	}
}
