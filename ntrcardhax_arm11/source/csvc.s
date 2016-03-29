.arm
.align 4

.macro SVC_BEGIN name
	.section .text.\name, "ax", %progbits
	.global \name
	.type \name, %function
	.align 2
\name:
.endm

SVC_BEGIN svcMapMemory
	push {r4}
	mov r4, #3
	svc 0x6d
	pop {r4}
	bx  lr