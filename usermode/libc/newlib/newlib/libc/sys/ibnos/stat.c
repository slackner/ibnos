#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <reent.h>
#include <errno.h>
#include <stdint.h>
#include "syscall.h"

int _stat_r(struct _reent *ptr, const char *__restrict file, struct stat *__restrict st)
{
	int32_t fileobj;
	memset(st, 0, sizeof(*st));

	fileobj = filesystemSearchFile(-1, file, strlen(file), 0);
	if (fileobj >= 0)
	{
		st->st_mode = S_IFREG;
		st->st_size = objectGetStatus(fileobj, 0);

		objectClose(fileobj);
		return 0;
	}

	fileobj = filesystemSearchDirectory(-1, file, strlen(file), 0);
	if (fileobj >= 0)
	{
		st->st_mode = S_IFDIR;

		objectClose(fileobj);
		return 0;
	}

	ptr->_errno = ENOENT;
	return -1;
}

int _fstat_r(struct _reent *ptr, int file, struct stat *st)
{
	ptr->_errno = 0;
	st->st_mode = S_IFCHR;
	return 0;
}