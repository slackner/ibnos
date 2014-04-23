#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <reent.h>
#include <errno.h>

int _fstat(struct _reent *ptr, int fd, struct stat *st)
{
	ptr->_errno = ENOMEM;
	return -1;
}
