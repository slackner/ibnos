#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>
#include "syscall.h"

int _unlink_r(struct _reent *ptr, const char *name)
{
	int32_t fileobj, ret;

	fileobj = filesystemSearchFile(-1, name, strlen(name), 0);
	if (fileobj < 0)
	{
		ptr->_errno = ENOENT;
		return -1;
	}

	ret = objectShutdown(fileobj, 0);
	objectClose(fileobj);

	if (ret < 0)
	{
		ptr->_errno = EACCES;
		return -1;
	}

	return 0;
}
