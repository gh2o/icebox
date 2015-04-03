.globl memset
memset:
	/* buffer in %rdi, value in %rsi, size in %rdx */
	mov %rdx, %rcx
	mov %rsi, %rax
	rep stosb
	ret
