#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <reent.h>
#include <errno.h>
#include <stdint.h>
#include "syscall.h"

int _fstat_r(struct _reent *ptr, int file, struct stat *st)
{
	ptr->_errno = 0;
	if (!st)
	{
		ptr->_errno = EFAULT;
		return -1;
	}

	memset(st, 0, sizeof(*st));
	st->st_mode = S_IFREG;
	st->st_size = objectGetStatus(file, 0);

	if (st->st_size < 0)
	{
		ptr->_errno = EBADF;
		return -1;
	}

	return 0;
}