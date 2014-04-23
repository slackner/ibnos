#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <reent.h>
#include <errno.h>

int _stat_r(struct _reent *ptr, const char *__restrict file, struct stat *__restrict st)
{
	ptr->_errno = ENOSYS;
	return -1;
}

int _fstat_r(struct _reent *ptr, int file, struct stat *st)
{
	ptr->_errno = 0;
	st->st_mode = S_IFCHR;
	return 0;
}