#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>
#include <unistd.h>
#include "syscall.h"

int _fork_r(struct _reent *ptr)
{
	pid_t pid;
	ptr->_errno = 0;

	pid = (pid_t)ibnos_syscall(SYSCALL_FORK);
	if (pid < 0)
		ptr->_errno = ENOMEM;

	return pid;
}
