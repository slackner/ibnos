#include <unistd.h>
#include <reent.h>
#include <errno.h>
#include "syscall.h"

int dup(int oldfd)
{
	int newfd;
	struct _reent *reent = __getreent();
	reent->_errno = 0;

	newfd = ibnos_syscall(SYSCALL_OBJECT_DUP, (uint32_t)oldfd);
	if (newfd < 0)
		reent->_errno = EBADF;

	return newfd;
}

int dup2(int oldfd, int newfd)
{
	int result;
	struct _reent *reent = __getreent();
	reent->_errno = 0;

	result = ibnos_syscall(SYSCALL_OBJECT_DUP2, (uint32_t)oldfd, (uint32_t)newfd);
	if (result < 0)
		reent->_errno = EBADF;

	return result;
}