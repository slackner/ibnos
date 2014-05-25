#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <fcntl.h>
#include <reent.h>
#include <errno.h>
#include <stdarg.h>
#include "syscall.h"

int _open_r(struct _reent *ptr, const char *file, int flags, int mode)
{
	int32_t fileobj, ret;

	fileobj = filesystemSearchFile(-1, file, strlen(file), (flags & O_CREAT));
	if (fileobj < 0)
	{
		ptr->_errno = EACCES;
		return -1;
	}

	ret = filesystemOpen(fileobj);
	objectClose(fileobj);

	ptr->_errno = (ret >= 0) ? 0 : EACCES;
	return ret;
}
