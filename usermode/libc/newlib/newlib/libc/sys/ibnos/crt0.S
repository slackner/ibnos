.section .text

.global _start
_start:

	# Init reent structures / stdio
	call __init_ibnos

	call __get_ibnos_envp
	push %eax
	call __get_ibnos_argv
	push %eax
	call __get_ibnos_argc
	push %eax

	call main
	addl $12, %esp

	# Run exit stuff
	pushl %eax
	call exit

	# Did not exit? Cause a GP!
	hlt

.global _thread_start
_thread_start:

	# Save arguments to stack
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax

	# Init thread reent structures / stdio
	call __init_ibnos_thread

	# Run entrypoint
	pop %eax
	call *%eax
	add $12, %esp

	# Shutdown thread properly
	mov %eax, %ebx
	mov $0x002, %eax # SYSCALL_EXIT_THREAD
	int $0x80

	# Did not exit? Cause a GP!
	hlt
