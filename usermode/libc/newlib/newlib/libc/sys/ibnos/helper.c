#include <_ansi.h>
#include <reent.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "syscall.h"

extern char **environ;

char **__get_ibnos_argv()
{
	static bool init = false;
	uint8_t *buf_argv = getProgramArguments();

	if (!init && buf_argv)
	{
		uint32_t *offset = (uint32_t *)buf_argv;
		uint32_t buflen_argv = getProgramArgumentsLength(); 
		buf_argv[buflen_argv - 1] = 0; /* add nullterminatation if necessary */

		while (buflen_argv >= sizeof(uint32_t))
		{
			if (!*offset || *offset >= buflen_argv)
			{
				*offset = 0;
				break;
			}

			*offset += (uint32_t)buf_argv;
			buflen_argv -= sizeof(uint32_t);
			offset++;
		}

		init = true;
	}

	return (char **)buf_argv;
}

uint32_t __get_ibnos_argc()
{
	uint32_t argc = 0;
	char **argv = __get_ibnos_argv();
	if (argv)
	{
		while (argv[argc]) argc++;
	}
	return argc;
}

char **__get_ibnos_envp()
{
	static bool init = false;
	uint8_t *buf_envp = getEnvironmentVariables();

	if (!init && buf_envp)
	{
		uint32_t *offset = (uint32_t *)buf_envp;
		uint32_t buflen_envp = getEnvironmentVariablesLength(); 
		buf_envp[buflen_envp - 1] = 0; /* add nullterminatation if necessary */

		while (buflen_envp >= sizeof(uint32_t))
		{
			if (!*offset || *offset >= buflen_envp)
			{
				*offset = 0;
				break;
			}

			*offset += (uint32_t)buf_envp;
			buflen_envp -= sizeof(uint32_t);
			offset++;
		}

		init = true;
	}

	return (char **)buf_envp;
}

void __init_ibnos_thread()
{
	struct _reent *reent = __getreent();
	_REENT_INIT_PTR(reent);
	__sinit(reent);
}

void __init_ibnos()
{
	__sinit(_GLOBAL_REENT);
	__init_ibnos_thread();
	char** tmpEnv = __get_ibnos_envp();
	if (tmpEnv) environ = tmpEnv;
}
