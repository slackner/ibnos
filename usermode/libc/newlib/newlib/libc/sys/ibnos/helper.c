#include <_ansi.h>
#include <reent.h>

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
}