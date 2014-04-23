#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <fcntl.h>
#include <reent.h>
#include <errno.h>
#include <stdarg.h>

int _open_r(struct _reent *ptr, const char *file, int flags, int mode)
{
	ptr->_errno = ENODEV;
	return -1;
}
