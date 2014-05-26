#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <reent.h>
#include <errno.h>
#include "syscall.h"

int _execve_r(struct _reent *ptr, const char *name, char *const *argv, char *const *envp)
{
	int32_t fileobj, ret = -1;
	uint32_t argc, envc;
	uint32_t buflen_argv, buflen_envp;
	char *buf_argv, *buf_envp;

	fileobj = filesystemSearchFile(-1, name, strlen(name), false);
	if (fileobj < 0)
	{
		ptr->_errno = ENOENT;
		return -1;
	}

	buflen_argv = sizeof(uint32_t);
	for (argc = 0; argv && argv[argc]; argc++)
		buflen_argv += sizeof(uint32_t) + strlen(argv[argc]) + 1;

	buflen_envp = sizeof(uint32_t);
	for (envc = 0; envp && envp[envc]; envc++)
		buflen_envp += sizeof(uint32_t) + strlen(envp[envc]) + 1;

	buf_argv = (char *)malloc(buflen_argv);
	if (buf_argv)
	{
		buf_envp = (char *)malloc(buflen_envp);
		if (buf_envp)
		{
			uint32_t *offsets, tmp;
			char *cur;

			offsets = (uint32_t *)buf_argv;
			cur = buf_argv + sizeof(uint32_t) * (argc + 1);
			while (argc--)
			{
				*offsets++ = (cur - buf_argv);
				tmp = strlen(*argv);
				memcpy(cur, *argv, tmp + 1);
				cur += (tmp + 1);
				argv++;
			}
			*offsets++ = 0;

			offsets = (uint32_t *)buf_envp;
			cur = buf_envp + sizeof(uint32_t) * (envc + 1);
			while (envc--)
			{
				*offsets++ = (cur - buf_envp);
				tmp = strlen(*envp);
				memcpy(cur, *envp, tmp + 1);
				cur += (tmp + 1);
				envp++;
			}
			*offsets++ = 0;

			/* now run the program and pass all the arguments */
			ret = executeProgram(fileobj, buf_argv, buflen_argv, buf_envp, buflen_envp);

			free(buf_envp);
		}
		free(buf_argv);
	}

	objectClose(fileobj);

	ptr->_errno = (ret >= 0) ? 0 : ENOEXEC;
	return ret;
}

int _execve(const char *name, char *const *argv, char *const *envp)
{
	return _execve_r(__getreent(), name, argv, envp);
}
