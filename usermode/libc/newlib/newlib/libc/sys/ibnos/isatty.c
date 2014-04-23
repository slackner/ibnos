#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>

int _isatty_r(struct _reent *ptr, int file)
{
  ptr->_errno = ENOSYS;
  return 0;
}
