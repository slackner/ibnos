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

	if (flags & O_DIRECTORY)
		fileobj = filesystemSearchDirectory(-1, file, strlen(file), 0);
	else
		fileobj = filesystemSearchFile(-1, file, strlen(file), (flags & O_CREAT));

	if (fileobj < 0)
	{
		ptr->_errno = ENOENT;
		return -1;
	}

	ret = filesystemOpen(fileobj);
	objectClose(fileobj);

	if (ret < 0)
	{
		ptr->_errno = EACCES;
		return -1;
	}

	/* truncate the file */
	if (flags & O_TRUNC)
	{
		objectSignal(ret, 0);
		objectShutdown(ret, 0);
	}

	/* move pointer at the end */
	if (flags & O_APPEND)
	{
		objectSignal(ret, objectGetStatus(ret, 0));
	}

	ptr->_errno = 0;
	return ret;
}
