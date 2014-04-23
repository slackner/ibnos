#include <unistd.h>
#include <reent.h>
#include <errno.h>
#include "syscall.h"

int pipe(int pipefd[2])
{
	struct _reent *reent = __getreent();
	reent->_errno = 0;

	if (!pipefd)
	{
		reent->_errno = EFAULT;
		return -1;
	}

	pipefd[0] = createPipe();
	if (pipefd[0] >= 0)
	{
		pipefd[1] = dup(pipefd[0]);

		if (pipefd[1] >= 0)
			return 0;
	}

	reent->_errno = EMFILE;
	return -1;
}