#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <sys/time.h>
#include <sys/times.h>
#include <reent.h>
#include <errno.h>

struct timeval;
struct timezone;

int _gettimeofday_r(struct _reent *ptr, struct timeval *ptimeval, void *ptimezone)
{
  ptr->_errno = ENOSYS;
  return -1;
}
