! A DC boot ROM replacement for Swirly
!
! Includes some routines by Marcus Comstedt.
!
! This is meant to serve as a replacement for the DC boot ROM to coerce programs
! that depend on the boot ROM to run (i.e. IP.BIN).  It will only work running on
! Swirly.
!
! The trapa instruction causes a break in execution on Swirly and does nothing else.
!
! To compile into flat binary format:
! sh4-linux-as -little -o main.o main.s
! sh4-linux-ld -EL --oformat binary -Ttext 0xa0000000 main.o -o fbr.bin

	.global _start
	.extern romfont
	.text

_start:
	! set up stack
	mov.l stack_addr, r15

	! display a message stating we're alive
	mova hello_msg, r0
	mov r0, r4
	mov.l send_str_addr, r0
	jsr @r0
	nop

	! fill in the syscall area of RAM with the address of
	! the error handler

	mov.l error_handler_addr, r1
	mov #0x8c, r0
	shll8 r0
	shll8 r0
	shll8 r0 ! r0 = 0x8c000000 = RAM base

	! r6 = counter, r7 = num times to go
	mov #0, r6
	mov #0x80, r7
	extu.b r7, r7
	shll2 r7
	shll2 r7
1:
	mov.l r1, @(r0, r6)
	add #4, r6
	dt r7
	bf 1b

	! now to fill in the syscalls we've implemented
	! TODO
	mov.l sce0_where, r0
	mov.l sce0_addr, r4
	mov.l r4, @r0
	
	mov.l scbc_where, r0
	mov.l scbc_addr, r4
	mov.l r4, @r0

	mov.l scb4_where, r0
	mov.l scb4_addr, r4
	mov.l r4, @r0

	mov.l ip_bin_start, r0
	jmp @r0
	nop


	.align 4
	stack_addr: .long 0x7e001000
	send_str_addr: .long send_str
	error_handler_addr: .long syscall_error_handler
	ip_bin_start: .long 0x8c008300
	sce0_where: .long 0x8c0000e0
	sce0_addr: .long syscall_8c0000e0
	scbc_where: .long 0x8c0000bc
	scbc_addr: .long syscall_gdrom
	scb4_where: .long 0x8c0000b4
	scb4_addr: .long syscall_8c0000b4

	hello_msg: .asciz "fakebootrom: Welcome to Swirly.\n"

	.align 4
syscall_gdrom: ! at 8c0000bc
	! do a Swirly hook
	.word 0xfffd ! undefined instruction means Swirly hook
	.word 1 ! type of hook (1 = GDROM syscall)

	rts
	nop


! return the address of our ROM font
	.align 4
syscall_8c0000b4:
	mov.l romfont_addr, r0
	rts
	nop

	.align 4
romfont_addr: .long romfont

! syscall - 8c0000e0
	.align 4
syscall_8c0000e0:
	mov #1, r5
	cmp/eq r5, r4
	bf 1f

	mova sce0_msg_bootmenu, r0
	mov r0, r4
	mov.l sce0_send_str_addr, r0
	jsr @r0
	nop

	mov.l sce0_jump_addr, r0
	jsr @r0
	nop

	trapa #0xe0
1:
	rts
	nop

	.align 4
	sce0_jump_addr: .long 0xa0000000
	sce0_send_str_addr: .long send_str
	sce0_msg_bootmenu: .asciz "fakebootrom: Just pretend we are returning to the boot menu\n"


! generic error handler for unimplemented syscalls
	.align 4
syscall_error_handler:
	mova seh_error_msg, r0
	mov r0, r4
	mov.l seh_send_str_addr, r0
	jsr @r0
	nop
	trapa #0xff

	.align 4
	seh_send_str_addr: .long send_str
	seh_error_msg: .asciz "fakebootrom: Unimplemented syscall\n"


! This routine by Marcus
! Send a NUL-terminated string to the serial port
!
! r4 = string
	.align 4
send_str:
	mov	#-24,r2
	shll16	r2
	add	#12,r2
.ssloop1:
	mov.b	@r4+,r1
	tst	r1,r1
	bt	.endstr
	extu.b	r1,r1
.ssloop2:
	mov.w	@(4,r2),r0
	tst	#0x20,r0
	bt	.ssloop2
	mov.b	r1,@r2
	nop
	and	#0x9f,r0
	bra	.ssloop1
	mov.w	r0,@(4,r2)
.endstr:
	nop
	rts

! This routine by Marcus
! Send a single character to the serial port
!
! r4 = character
send:
	mov	#-24,r2
	shll16	r2
	add	#12,r2
.sloop:	
	mov.w	@(4,r2),r0
	tst	#0x20,r0
	bt	.sloop
	mov.b	r4,@r2
	nop
	and	#0x9f,r0
	rts
	mov.w	r0,@(4,r2)

