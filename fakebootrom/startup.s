	.globl	start, _exitToIpBin, _load1stread
	.text

start:	
	! set up the stack
	mov.l stack_addr, r15

	stc	sr,r0
	or	#0xf0,r0
	ldc	r0,sr
	! First, make sure to run in the P2 area
	mov.l	setup_cache_addr,r0
	mov.l	p2_mask,r1
	or	r1,r0
	jmp	@r0
	nop
setup_cache:
	! Now that we are in P2, it's safe
	! to enable the cache
	mov.l	ccr_addr,r0
	mov.w	ccr_data,r1
	mov.l	r1,@r0
	! After changing CCR, eight instructions
	! must be executed before it's safe to enter
	! a cached area such as P1
	mov.l	initaddr,r0	! 1
	mov	#0,r1		! 2
	nop			! 3
	nop			! 4
	nop			! 5
	nop			! 6
	nop			! 7
	nop			! 8
	jmp	@r0		! go
	mov	r1,r0


	! Clear BSS section
init:
	mova hellomsg, r0
	mov r0, r4
	mov.l send_str_addr, r0
	jsr @r0
	nop

	mov.l	bss_start_addr,r0
	mov.l	bss_end_addr,r2
	mov	#3,r1
	add	r1,r0
	add	r1,r2
	not	r1,r1
	and	r1,r0
	and	r1,r2
	sub	r0,r2
	shlr	r2
	shlr	r2
	mov	#0,r1
.loop:	dt	r2
	mov.l	r1,@r0
	bf/s	.loop
	add	#4,r0

	mova mainInit, r0
	jsr @r0
	nop

	mov	#0,r2
	mov	#0,r1
	mov.l	mainaddr,r0
	jmp	@r0
	mov	r1,r0

	
	.align	4
stack_addr: .long 0x7e001000
send_str_addr: .long send_str
mainaddr:	.long	_main
initaddr:	.long	init
bss_start_addr:	.long	__bss_start
bss_end_addr:	.long	_end
p2_mask:	.long	0xa0000000
setup_cache_addr:	.long	setup_cache
ccr_addr:	.long	0xff00001c
ccr_data:	.word	0x090b
.align 4
hellomsg: .asciz "fakebootrom initializing...\n"

! Does various initialization
.align 4
mainInit:
	! now do syscall initialization
	! fill the first part of RAM with the address of our error message routine
	! so we can catch syscalls we don't know about yet
	mov.l syscallErrHandler_addr, r1
	mov.l ramBase, r0

	mov #0x80, r7
	extu.b r7, r7
	shll2 r7
	shll2 r7
	mov #0, r6
2:
	mov.l r1, @(r0, r6)
	add #4, r6
	dt r7
	bf 2b

	! setup system calls in RAM
	mov.l ramBase, r0

	! syscall 0x8c0000b4
	mov.l getRomFont_addr, r1
	mov #0xb4, r2
	extu.b r2, r2
	mov.l r1, @(r0, r2)

	! syscall 0x8c0000bc
	! GDROM stuff
	mov.l handleGdrom_addr, r1
	mov #0xbc, r2
	extu.b r2, r2
	mov.l r1, @(r0, r2)

	! syscall 0x8c0000e0
	! system stuff
	mov.l syscall_e0_addr, r1
	mov #0xe0, r2
	extu.b r2, r2
	mov.l r1, @(r0, r2)

	rts
	nop
.align 4
ramBase: .long 0x8c000000

syscallErrHandler_addr: .long syscallErrHandler
handleGdrom_addr: .long handleGdrom
getRomFont_addr: .long getRomFont
syscall_e0_addr: .long syscall_e0

! XXX: this returns junk at the moment
getRomFont: ! syscall b4, r1=0
	mov.l romFont_addr, r0	! this is pc-relative; can't put it in a delay slot
	rts
	nop
		
	.align 4
romFont_addr: .long romFont


syscallErrHandler:
	mova errMessage, r0
	mov r0, r4
	mov.l sendstr_addr, r0
	jsr @r0
	nop

0:
	bra 0b
	nop

.align 4
sendstr_addr: .long _sendstr
errMessage: .asciz "fakebootrom: Unimplemented syscall called.\n"

.align 4
! handles gdrom stuff
handleGdrom:
	! hook into Swirly, using the undefined instruction
	.word 0xfffd
	.word 1 ! type of hook (1=GDROM syscall)
	
	rts
	nop
.align 4
! hg_send_str_addr: .long send_str
! hg_msg1: .asciz "handleGdrom called.\n"

.align 4
! handles exiting the ip.bin and jumping to 1st_read.bin
syscall_e0:
	mov #1, r5
	cmp/eq r5, r4
	bf 1f
	mov.l jumpAddr, r0
	jsr @r0
	nop

	mova .e0_msg1, r0
	mov r0, r4
	mov.l .e0_send_str_addr, r0
	jsr @r0
	nop

2:
	! go into an infinite loop when 1st_read.bin returns, so we can see
	! its results on the screen
	bra 2b
	nop

1:
	rts
	nop
.align 4
jumpAddr: .long 0x8c010000
.e0_send_str_addr: .long send_str
.e0_msg1: .asciz "fakebootrom: User's program has exited. (syscall_e0)\n"

	! This routine by Marcus
	! Send a NUL-terminated string to the serial port
	!
	! r4 = string
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

! ***************** Functions callable from C modules *****************

_exitToIpBin:
	mov.l ipBin_addr, r0
	jmp @r0
	nop
.align 4
ipBin_addr: .long 0x8c008300

_load1stread:
	! Swirly hook
	! trapa #0xcd
	.word 0xfffd
	.word 2 ! 2 = load 1ST_READ.BIN while descrambling

	rts
	nop
	

! ******************* DATA *********************
! I'm putting it here because I don't know how to do this a better way.
! Also I'm lazy.

.align 4
romFont:
.fill 9180, 1, 0xff

	.end

