#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <sys/times.h>
#include <reent.h>
#include <errno.h>

clock_t _times_r(struct _reent *ptr, struct tms *buf)
{
  ptr->_errno = ENOSYS;
  return -1;
}
