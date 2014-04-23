#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>

int _unlink_r(struct _reent *ptr, const char *name)
{
  ptr->_errno = ENOSYS;
  return -1;
}
