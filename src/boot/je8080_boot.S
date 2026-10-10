; JE-8080 synthetic boot block for the JP-8080 application image.
;
; The JP-8080 firmware image (the 896 KB .bin, or what jeLib assembles from the
; 14-file .mid update) is the APPLICATION only, linked to run at CPU 0x10000.
; The 64 KB below it in the real flash -- the boot block -- is not in it. The
; application still depends on that block for four things:
;
;   1. the CPU's vector table and reset code (reset enters the app at 0x10100)
;   2. the SCI0 ERI/RXI handlers (vectors 52/53): MIDI receive
;   3. six compiler runtime helpers at 0xA000-0xA0D2 (about 2,000 call sites)
;   4. the init routine of task 3 in the app's task table (0x587E)
;
; This file provides all four. Task 3 also calls at least seven more
; boot-block routines whose behaviour is unknown; they return immediately. It is written from what the application expects
; of each entry point (arguments, results, which registers survive, stack
; layout), worked out from the call sites and from the JP-8000's matching
; library at 0x3000. No byte of it is copied from a Roland ROM.
;
; Assemble and regenerate the C header with tools/gen_boot_header.sh
; (binutils-h8300-hms). The build itself only needs the generated header.

        .h8300s
        .text

APP_BASE        = 0x10000
APP_ENTRY       = 0x10100       ; C runtime entry
APP_SCI_RXI     = 0x10200       ; expects ER0 already pushed
APP_SCI_ERI     = 0x10204       ; expects ER0 already pushed
BOOT_STACK      = 0x20F000

; ---------------------------------------------------------------- vectors
; Vector n points at the application's stub n (APP_BASE + 4n), which jumps to
; the real handler. Reset and the two SCI0 vectors are ours.
        .org    0x0000
vectors:
        .long   reset                           ; 0  reset
        .long   APP_BASE + 4*1,  APP_BASE + 4*2,  APP_BASE + 4*3
        .long   APP_BASE + 4*4,  APP_BASE + 4*5,  APP_BASE + 4*6,  APP_BASE + 4*7
        .long   APP_BASE + 4*8,  APP_BASE + 4*9,  APP_BASE + 4*10, APP_BASE + 4*11
        .long   APP_BASE + 4*12, APP_BASE + 4*13, APP_BASE + 4*14, APP_BASE + 4*15
        .long   APP_BASE + 4*16, APP_BASE + 4*17, APP_BASE + 4*18, APP_BASE + 4*19
        .long   APP_BASE + 4*20, APP_BASE + 4*21, APP_BASE + 4*22, APP_BASE + 4*23
        .long   APP_BASE + 4*24, APP_BASE + 4*25, APP_BASE + 4*26, APP_BASE + 4*27
        .long   APP_BASE + 4*28, APP_BASE + 4*29, APP_BASE + 4*30, APP_BASE + 4*31
        .long   APP_BASE + 4*32, APP_BASE + 4*33, APP_BASE + 4*34, APP_BASE + 4*35
        .long   APP_BASE + 4*36, APP_BASE + 4*37, APP_BASE + 4*38, APP_BASE + 4*39
        .long   APP_BASE + 4*40, APP_BASE + 4*41, APP_BASE + 4*42, APP_BASE + 4*43
        .long   APP_BASE + 4*44, APP_BASE + 4*45, APP_BASE + 4*46, APP_BASE + 4*47
        .long   APP_BASE + 4*48, APP_BASE + 4*49, APP_BASE + 4*50, APP_BASE + 4*51
        .long   sci0_eri                        ; 52 SCI0 receive error
        .long   sci0_rxi                        ; 53 SCI0 receive data (MIDI in)
        .long   APP_BASE + 4*54, APP_BASE + 4*55
        .long   APP_BASE + 4*56, APP_BASE + 4*57, APP_BASE + 4*58, APP_BASE + 4*59
        .long   APP_BASE + 4*60, APP_BASE + 4*61, APP_BASE + 4*62, APP_BASE + 4*63

; ---------------------------------------------------------------- reset
        .org    0x0200
reset:
        mov.l   #BOOT_STACK, er7        ; the app sets its own SP shortly after
        jmp     @APP_ENTRY:24

; ---------------------------------------------------------------- SCI0
; At the addresses the app's own stubs 52/53 jump to, so both paths agree.
; Normal-operation behaviour of a boot loader's SCI handler: save ER0 and
; enter the app's handler, which pops it.
        .org    0x235C
sci0_eri:
        push.l  er0
        jmp     @APP_SCI_ERI:24

        .org    0x236E
sci0_rxi:
        push.l  er0
        jmp     @APP_SCI_RXI:24

; ---------------------------------------------------------------- data
; The app's C runtime copies these ranges out of the boot block into RAM
; (0x207518-0x20755A). Their real contents are unknown; zero, not RTS fill.
        .org    0x29D2
        .fill   64, 1, 0
        .org    0x8EAC
        .fill   2, 1, 0

; ---------------------------------------------------------------- unknown
; Boot-block routines the app calls whose real behaviour is unknown, mostly
; from task 3 (0x5D4A0-0x5D8DC) and one from 0x56B84. Each returns at once.
; Every other unused byte of this block is RTS fill too (tools/gen_boot_header.sh),
; so a call to any address we have not met returns instead of sliding through
; empty memory into whatever code comes next.
        .org    0x3908
        rts                             ; from 0x5D4A0
        .org    0x3A36
        rts                             ; from 0x5D4E4
        .org    0x3A64
        rts                             ; from 0x5D51E
        .org    0x43EC
        rts                             ; from 0x5D77C
        .org    0x44AA
        rts                             ; from 0x5D800

; ---------------------------------------------------------------- task 3
; The app's task table (v1.04: 0x5E4D8) gives task 3 an init routine in the
; boot block. Its task body is in the app; the init is a no-op here.
        .org    0x587E
task3_init:
        rts

        .org    0x8C28
        rts                             ; from 0x56B84, 0x5D87E (task 3)
        .org    0x8C86
        rts                             ; from 0x5D8D0, 0x5D8DC (task 3)

; ---------------------------------------------------------------- runtime
; Entry points the application calls. Each is a JMP to the implementation
; below, so the entries keep their addresses whatever the bodies' sizes.
        .org    0xA000
        jmp     @sdiv32:24              ; ER0 = ER0 / ER1, ER1 = ER0 % ER1 (signed)
        .org    0xA026
        jmp     @udiv32:24              ; ER0 = ER0 / ER1, ER1 = ER0 % ER1 (unsigned)
        .org    0xA062
        jmp     @mul32:24               ; ER0 = ER0 * ER1 (low 32 bits)
        .org    0xA082
        jmp     @copy:24                ; copy ER2 bytes from @ER0 to @ER1
        .org    0xA0AA
        jmp     @prologue:24            ; JSR'd at function entry: save ER2-ER6
        .org    0xA0D2
        jmp     @epilogue:24            ; JMP'd at function exit: restore, return

        .org    0xB000

; Signed 32-bit divide. Quotient truncates toward zero; the remainder takes
; the dividend's sign (C semantics). Preserves everything but ER0/ER1.
sdiv32:
        push.l  er2
        push.l  er3
        mov.l   er0, er3                ; er3: dividend (its sign is the remainder's)
        mov.l   er0, er2
        xor.l   er1, er2                ; er2: bit 31 = quotient sign
        mov.l   er0, er0
        bpl     1f
        neg.l   er0
1:      mov.l   er1, er1
        bpl     2f
        neg.l   er1
2:      jsr     @udiv32:24
        mov.l   er3, er3
        bpl     3f
        neg.l   er1
3:      mov.l   er2, er2
        bpl     4f
        neg.l   er0
4:      pop.l   er3
        pop.l   er2
        rts

; Unsigned 32-bit divide by shift-and-subtract. Preserves everything but
; ER0/ER1. Division by zero yields 0xFFFFFFFF and the dividend as remainder.
udiv32:
        push.l  er2
        push.l  er3
        mov.l   er1, er3                ; er3: divisor
        sub.l   er1, er1                ; er1: running remainder
        mov.b   #32, r2l
1:      shll.l  er0                     ; next dividend bit into C, quotient grows from bit 0
        rotxl.l er1
        bcs     2f                      ; remainder overflowed 32 bits: certainly >= divisor
        cmp.l   er3, er1
        blo     3f
2:      sub.l   er3, er1
        adds    #1, er0
3:      dec.b   r2l
        bne     1b
        pop.l   er3
        pop.l   er2
        rts

; 32 x 32 -> low 32 bits. Preserves everything but ER0.
mul32:
        push.l  er2
        push.l  er3
        mov.w   e0, r2
        mulxu.w r1, er2                 ; a.hi * b.lo
        mov.w   e1, r3
        mulxu.w r0, er3                 ; b.hi * a.lo
        mulxu.w r1, er0                 ; a.lo * b.lo
        add.w   r2, e0
        add.w   r3, e0
        pop.l   er3
        pop.l   er2
        rts

; Byte copy, ER2 bytes from @ER0 to @ER1. Preserves every register.
copy:
        push.l  er0
        push.l  er1
        push.l  er2
        push.w  r3
        mov.l   er2, er2
        beq     2f
1:      mov.b   @er0+, r3l
        mov.b   r3l, @er1
        adds    #1, er1
        dec.l   #1, er2
        bne     1b
2:      pop.w   r3
        pop.l   er2
        pop.l   er1
        pop.l   er0
        rts

; Called with JSR at function entry. On return the stack holds, from SP up:
; ER2, ER3, ER4, ER5, ER6, then the function's own return address -- and no
; register has changed.
prologue:
        push.l  er5
        push.l  er4
        push.l  er3
        push.l  er2                     ; SP -> ER2 ER3 ER4 ER5 ret_func ret_caller
        mov.l   @(16, er7), er2         ; er2 = ret_func
        mov.l   er6, @(16, er7)         ; that slot now holds ER6
        push.l  er2                     ; return into the function
        mov.l   @(4, er7), er2          ; put ER2 back
        rts

; Reached with JMP at function exit, stack as prologue left it.
epilogue:
        pop.l   er2
        pop.l   er3
        pop.l   er4
        pop.l   er5
        pop.l   er6
        rts

        .end
