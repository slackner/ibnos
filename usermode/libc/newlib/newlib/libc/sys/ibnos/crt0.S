.section .text
 
.global _start
_start:

	# Init reent structures / stdio
	call __init_ibnos

	# Save argc and argv., then call main
	pushl $0
	pushl $0
	call main
	addl $8, %esp

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
	call %eax
	add $12, %esp

	# Shutdown thread properly
	mov %eax, %ebx
	mov $2, %eax # SYSCALL_EXIT_THREAD
	int $0x80

	# Did not exit? Cause a GP!
	hlt
