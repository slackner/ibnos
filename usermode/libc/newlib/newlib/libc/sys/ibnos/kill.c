#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>

int _kill_r(struct _reent *ptr, int pid, int sig)
{
  ptr->_errno = ENOSYS;
  return -1;
}
