#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <unistd.h>
#include <reent.h>
#include <errno.h>

off_t _lseek_r(struct _reent *ptr, int file, off_t pos, int whence)
{
	ptr->_errno = EINVAL;
	return - 1;
}
