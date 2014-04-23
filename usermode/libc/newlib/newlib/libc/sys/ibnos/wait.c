#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>

int _wait_r(struct _reent *ptr, int *status)
{
  ptr->_errno = ENOSYS;
  return -1;
}
