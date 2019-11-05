        @
        @ $Id: hostfsfiler.s,v 1.3 2008/12/08 20:30:30 mhowkins Exp $
        @
        @ HostFS Filer
        @

        @ Register naming
        wp .req r12

        @ ARM constants
        VBIT = 1 << 28
        CBIT = 1 << 29
        ZBIT = 1 << 30
        NBIT = 1 << 31

        @ RISC OS constants
        XOS_CLI               = 0x20005
        XOS_Exit              = 0x20011
        XOS_Module            = 0x2001e
        XOS_ReadModeVariable  = 0x20035
        XOS_ReadMonotonicTime = 0x20042
        XWimp_Initialise = 0x600c0
        XWimp_CreateIcon = 0x600c2
	XWimp_CreateMenu = 0x600d4
        XWimp_CloseDown  = 0x600dd
        XWimp_PollIdle   = 0x600e1
        XWimp_SpriteOp   = 0x600e9

        Module_Enter = 2
        Module_Claim = 6
        Module_Free  = 7

        Message_Quit = 0

        Service_Reset             = 0x27
        Service_StartFiler        = 0x4b
        Service_StartedFiler      = 0x4c
        Service_FilerDying        = 0x4f

        SpriteOp_ReadSpriteInfo = 40

        ModeVariable_YEig = 5

        WIMP_VERSION = 300

        WIMP_POLL_MASK = 0x00000031     @ no Null, Pointer Entering or Pointer Leaving events

        WORKSPACE_SIZE = 512

        WS_MY_TASK_HANDLE      = 0
        WS_FILER_TASK_HANDLE   = 4
        WS_WIMP_VERSION        = 8
        WS_ICON_BAR_BLOCK      = 12
        WS_WIMP_BLOCK          = 48 @ must be last



        .global _start

_start:

module_start:

        .int	start           @ Start
        .int	init            @ Initialisation
        .int	final           @ Finalisation
        .int    service_pre     @ Service Call
        .int	modtitle        @ Title String
        .int	help            @ Help String
        .int	table           @ Help and Command keyword table
        .int	0               @ SWI Chunk base
        .int	0               @ SWI handler code
        .int	0               @ SWI decoding table
        .int	0               @ SWI decoding code
        .int    0               @ Message File
        .int    modflags        @ Module Flags

modflags:
        .int    1               @ 32 bit compatible

modtitle:
        .string	"RPCEmuHostFSFiler"

help:
        .string	"HostFSFiler\t0.05 (23 Sep 2014)"
        .align


        @ Help and Command keyword table
table:
desktop_hostfsfiler:
        .string "Desktop_HostFSFiler"
        .align
        .int    command_desktop_hostfsfiler
        .int    0x00070000
        .int    0
        .int    command_desktop_hostfsfiler_help

        .byte   0       @ Table terminator

command_desktop_hostfsfiler_help:
        .string	"The HostFSFiler provides the HostFS icons on the icon bar, and uses the Filer to display HostFS directories.\rDo not use *Desktop_HostFSFiler, use *Desktop instead."

        .align



init:
	stmfd	sp!, {lr}

	@ See if we need to claim some workspace
	ldr	r0, [r12]
	teq	r0, #0
	bne	1f

	@ Claim some workspace
	mov	r0, #Module_Claim
	mov	r3, #WORKSPACE_SIZE
	swi	XOS_Module
	ldmvsfd	sp!, {pc}       @ no memory claimed then refuse to initialise
	
	str	r2, [r12]
1:
	ldr	wp, [r12]

	@ Initialise the workspace
	mov	r0, #0
	str	r0, [wp, #WS_MY_TASK_HANDLE]

	ldmfd	sp!, {pc}



final:
	stmfd	sp!, {lr}

	ldr	wp, [r12]

	@ Close Wimp task if active
	ldr	r0, [wp, #WS_MY_TASK_HANDLE]
	cmp	r0, #0
	ldrgt	r1, TASK
	swigt	XWimp_CloseDown

	@ Free workspace
	mov	r0, #Module_Free
	mov	r2, r12
	swi	XOS_Module

        @ Clear V flag (26/32 bit safe) so our module will die
        cmp     pc, #0          @ Clears V (also clears N, Z, and sets C)
	ldmfd	sp!, {pc}



         @ RISC OS 4 Service codetable
service_codetable:
        .int    0               @ no special flags enabled
        .int    service_main
        .int    Service_Reset
        .int    Service_StartFiler
        .int    Service_StartedFiler
        .int    Service_FilerDying
        .int    0               @ table terminator
        .int    service_codetable
service_pre:
        mov     r0, r0          @ magic instruction, pointer to service table at service_pre-4
        teq     r1, #Service_Reset
        teqne   r1, #Service_StartFiler
        teqne   r1, #Service_StartedFiler
        teqne   r1, #Service_FilerDying
        movne   pc, lr

service_main:
        stmfd   sp!, {lr}

        ldr     wp, [r12]

        teq     r1, #Service_Reset
        beq     service_reset
        teq     r1, #Service_StartFiler
        beq     service_startfiler
        teq     r1, #Service_StartedFiler
        beq     service_startedfiler
        teq     r1, #Service_FilerDying
        beq     service_filerdying

        @ Should never reach here
        ldmfd   sp!, {pc}



service_reset:
	@ Zero the Task Handle
	mov	r14, #0
	str	r14, [wp, #WS_MY_TASK_HANDLE]
	ldmfd	sp!, {pc}



service_startfiler:
	ldr	r14, [wp, #WS_MY_TASK_HANDLE]
	teq	r14, #0                                 @ Am I already active?
	moveq	r14, #-1                                @ No, so set handle to -1
	streq	r14, [wp, #WS_MY_TASK_HANDLE]
	streq	r1,  [wp, #WS_FILER_TASK_HANDLE]        @ store Filer's task handle
	adreq	r0,  desktop_hostfsfiler                @ r0 points to command to start task
	moveq	r1,  #0                                 @ claim the service
	ldmfd	sp!, {pc}



service_startedfiler:
	@ Zero the Task Handle if it is -1
	ldr	r14, [wp, #WS_MY_TASK_HANDLE]
	cmp	r14, #-1
	moveq	r14, #0
	streq	r14, [wp, #WS_MY_TASK_HANDLE]
	ldmfd	sp!, {pc}



service_filerdying:
        @ Shut down task if active

        stmfd	sp!, {r0-r1}

        ldr	r0, [wp, #WS_MY_TASK_HANDLE]
        cmp	r0, #0

        @ Zero the Task Handle if non-zero
        movne	r14, #0
        strne	r14, [wp, #WS_MY_TASK_HANDLE]

        @ Shut down task if Task Handle was positive
        ldrgt	r1, TASK
        swigt	XWimp_CloseDown

        ldmfd	sp!, {r0-r1}
        ldmfd	sp!, {pc}



command_desktop_hostfsfiler:
	stmfd	sp!, {lr}
	mov	r2, r0
	adr	r1, modtitle
	mov	r0, #Module_Enter
	swi	XOS_Module
	ldmfd	sp!, {pc}



TASK:
	.ascii	"TASK"

task_modtitle:
	.string	"HostFS Filer"
	.align

icon_bar_block:
	.int	-5		@ Left side of icon bar, scan from left (RO3+)
	.int	0		@ Minimum X
	.int	-16		@ Minimum Y
	.int	96		@ Maximum X
	.int	20		@ Maximum Y (excludes Sprite - added later)
	.int	0x1700310b	@ Flags (includes Indirected Text and Sprite)
	.int	0		@ Gap for pointer to Text
	.int	0		@ Gap for pointer to Validation String
	.int	6		@ Length of Text buffer

icon_bar_text:
	.string	"HostFS"

icon_bar_validation:
	.ascii	"S"		@ Unterminated - continues below...
icon_bar_icon_name:
	.string	"harddisc"

	.align



menu:
	.string	"HostFS"	@ Menu Title, padded to 12 bytes
	.align
	.int	0

	.byte	7, 2, 7, 0	@ Title colours
	.int	16 * 6		@ Width
	.int	44		@ Height
	.int	0		@ Vertical gap
	@ Menu items
	.int	(1 << 7)	@ Flags: last item
	.int	-1		@ Submenu pointer
	.int	0x07000001	@ Menu item icon flags, Text
	.string	"Free"		@ Menu item icon data, padded to 12 bytes
	.align
	.int	0



	@ "Start" entry point
	@ Entered in User Mode
	@ Therefore no need to preserve link register before calling SWIs
start:
        ldr     wp, [r12]               @ Get workspace pointer
        ldr     r0, [wp, #WS_MY_TASK_HANDLE]
        cmp     r0, #0                  @ Am I already active?
        ble     start_skipclosedown     @ No then skip following instructions
        ldr     r1, TASK                @ Yes, so close down first
        swi     XWimp_CloseDown
        mov     r0, #0                  @ Mark as inactive
        str     r0, [wp, #WS_MY_TASK_HANDLE]

start_skipclosedown:
	ldr	r0, = WIMP_VERSION	@ (re)start the task
	ldr	r1, TASK
	adr	r2, task_modtitle
	swi	XWimp_Initialise
	swivs	XOS_Exit		@ Exit if error

	str	r0, [wp, #WS_WIMP_VERSION]	@ store Wimp version
	str	r1, [wp, #WS_MY_TASK_HANDLE]	@ store Task handle


	@ Prepare block for Icon Bar icon
	adr	r0, icon_bar_block
	add	r1, wp, #WS_ICON_BAR_BLOCK

	ldmia	r0, {r2-r10}
	adr	r8, icon_bar_text		@ Fill in pointers
	adr	r9, icon_bar_validation
	stmia	r1, {r2-r10}

	@ Calculate size of Icon Bar Icon
	mov	r0, #SpriteOp_ReadSpriteInfo
	adr	r2, icon_bar_icon_name
	swi	XWimp_SpriteOp
	movvc	r0, r6
	movvc	r1, #ModeVariable_YEig
	swivc	XOS_ReadModeVariable
	bvs	close_down

	@ Add sprite height to Maximum Y of icon's Bounding Box
	ldr	r0, [r12, #WS_ICON_BAR_BLOCK + 16]
	add	r0, r0, r4, lsl r2	@ += Pixels << YEig
	str	r0, [r12, #WS_ICON_BAR_BLOCK + 16]

	@ Create Icon on Icon Bar
	mov	r0, #0x71000000		@ Priority higher than ADFS Hard Disc but lower than CD-ROM discs
	add	r1, wp, #WS_ICON_BAR_BLOCK
	swi	XWimp_CreateIcon
	bvs	close_down


	@ Main poll loop
re_poll:
	swi	XOS_ReadMonotonicTime	@ returns time in r0
	add	r2, r0, #100		@ poll no sooner than 1 sec unless event
	ldr	r0, = WIMP_POLL_MASK
	add	r1, wp, #WS_WIMP_BLOCK	@ point to Wimp block within workspace
	swi	XWimp_PollIdle
	bvs	close_down

	teq	r0, #6			@ 6 = Mouse Click
	beq	mouse_click
	teq	r0, #9			@ 9 = Menu Selection
	beq	menu_selection
	teq	r0, #17			@ 17 = User Message
	teqne	r0, #18			@ 18 = User Message Recorded
	beq	user_message
	b	re_poll


mouse_click:
	ldr	r0, [r1, #12]		@ Icon handle
	cmp	r0, #-2
	bne	re_poll

	ldr	r0, [r1, #8]		@ Buttons

	cmp	r0, #4			@ Select
	cmpne	r0, #1			@ Adjust
	adreq	r0, cli_command
	swieq	XOS_CLI
	beq	re_poll

	cmp	r0, #2			@ Menu
	bne	re_poll

	ldr	r2, [r1, #0]		@ X coordinate of click
	sub	r2, r2, #64
	mov	r3, #(96 + 44)
	adr	r1, menu
	swi	XWimp_CreateMenu

	b	re_poll

cli_command:
	.string	"Filer_OpenDir HostFS::HostFS.$"
	.align


menu_selection:
	adr	r0, free_cli_command
	swi	XOS_CLI

	b	re_poll

free_cli_command:
	.string	"ShowFree -fs HostFS HostFS"
	.align


user_message:
	ldr	r0, [r1, #16]		@ Contains message code
	teq	r0, #Message_Quit	@ Is it Quit message...?
	bne	re_poll			@ ...no so re-poll
					@ otherwise continue to...
close_down:
	@ Close down Wimp task
	ldr	r0, [wp, #WS_MY_TASK_HANDLE]
	ldr	r1, TASK
	swi	XWimp_CloseDown

	@ Zero the Task Handle
	mov	r0, #0
	str	r0, [wp, #WS_MY_TASK_HANDLE]

	swi	XOS_Exit
