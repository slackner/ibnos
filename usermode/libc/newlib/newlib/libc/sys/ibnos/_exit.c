#include <stdlib.h>
#include "syscall.h"

void _exit(int status)
{
	ibnos_syscall(SYSCALL_EXIT_PROCESS, status);
}