.section .text

.global _start
_start:

	# Init reent structures / stdio
	call __init_ibnos

	mov $0x103, %eax # SYSCALL_GET_ENVIRONMENT_VARIABLES
	int $0x80
	pushl %eax
	orl %eax, %eax
	jz .endloop1
		movl %eax, %edx
	.loop1:
		cmpl $0, 0(%edx)
		jz .endloop1
		addl %eax, 0(%edx)
		addl $4, %edx
		jmp .loop1
	.endloop1:

	mov $0x102, %eax # SYSCALL_GET_PROGRAM_ARGUMENTS
	int $0x80
	pushl %eax
	xorl %ecx, %ecx
	orl %eax, %eax
	jz .endloop2
		movl %eax, %edx
	.loop2:
		cmpl $0, 0(%edx)
		jz .endloop2
		addl %eax, 0(%edx)
		addl $4, %edx
		incl %ecx
		jmp .loop2
	.endloop2:
	
	pushl %ecx

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
