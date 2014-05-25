#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <unistd.h>
#include <reent.h>
#include <errno.h>
#include "syscall.h"

off_t _lseek_r(struct _reent *ptr, int file, off_t pos, int whence)
{
	int32_t ret;

	if (whence == SEEK_CUR)
	{
		int32_t offset = objectGetStatus(file, 1);
		if (offset == -1)
		{
			ptr->_errno = EBADF;
			return -1;
		}
		pos += offset;
	}
	else if (whence == SEEK_END)
	{
		int32_t offset = objectGetStatus(file, 0);
		if (offset == -1)
		{
			ptr->_errno = EBADF;
			return -1;
		}
		pos += offset;
	}

	ret = objectSignal(file, pos);
	ptr->_errno = (ret < 0) ? EINVAL : 0;
	return ret;
}
