[bits 32]

section .text

global shfpu_setContext, shfpu_sFLOAT, shfpu_sFMUL
extern shfpu_sop0, shfpu_sop1, shfpu_sresult
extern shfpu_dop0, shfpu_dop1, shfpu_dresult

shfpu_setContext:
	push ebp
	mov ebp, esp
	
	mov eax, [ebp + 8]
	mov ebx, [ebp + 12]
	mov ecx, [ebp + 16]
	mov edx, [ebp + 20]
	mov [FR], eax
	mov [XF], ebx
	mov [FPUL], ecx
	mov [FPSCR], edx
	
	leave
	ret
	
shfpu_sFMUL:
	fld dword [shfpu_sop0]
	fld dword [shfpu_sop1]
	fmulp st1
	fstp dword [shfpu_sresult]
	ret
	
shfpu_sFLOAT:
	mov eax, [FPUL]
	fild dword [eax]
	fstp dword [shfpu_sresult]
	ret
	
section .data
FR dd 0
XF dd 0
FPUL dd 0
FPSCR dd 0


	
