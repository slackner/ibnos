#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>

int _execve_r(struct _reent *ptr, const char *name, char *const *argv, char *const *env)
{
  ptr->_errno = ENOSYS;
  return -1;
}
