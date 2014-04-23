#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>

int _link_r(struct _reent *ptr, const char *existing, const char *new)
{
  ptr->_errno = ENOSYS;
  return -1;
}
