/* Routines to help out with SH FPU implementation */
.text
.global shfpu_setContext, shfpu_sFLOAT, shfpu_sFMUL
.extern shfpu_sop0, shfpu_sop1, shfpu_sresult
.extern shfpu_dop0, shfpu_dop1, shfpu_dresult

shfpu_setContext:
	pushl %ebp
	mov %esp, %ebp
	
	movl 8(%ebp), %eax
	movl 12(%ebp), %ebx
	movl 16(%ebp), %ecx
	movl 20(%ebp), %edx
	movl %eax, (FR)
	movl %ebx, (XF)
	movl %ecx, (FPUL)
	movl %edx, (FPSCR)
	
	leave
	ret

// slower to put the fmul here instead of letting g++ make one in SHCpu...
shfpu_sFMUL:
	fldl (shfpu_sop0)
	fldl (shfpu_sop1)
	fmulp %st(0), %st(1)
	fstpl (shfpu_sresult)
	ret

shfpu_sFLOAT:
	movl (FPUL), %eax
	fildl (%eax)
	fstpl (shfpu_sresult)
	ret

.data
.align 4
FR: .long 0
XF: .long 0
FPUL: .long 0
FPSCR: .long 0
