.syntax unified
.arm
.text
.align 2

.global traceAutoload
traceAutoload:
    push {r0-r12, lr}
    ldr r3, =0x02000A1C
    bl traceCallR3
    bl traceArm9Install
    pop {r0-r12, pc}
traceCallR3:
    bx r3

.global traceBlobStart
.global traceBlobEnd
.global traceBlobIRQ
.global traceBlobCommand
.global traceBlobOverlay
.global traceBlobOriginalIRQ
traceBlobStart:
traceBlobIRQ:
    push {r0-r3, r12, lr}
    ldr r2, traceMailbox
    ldr r0, [sp, #44]
    str r0, [r2, #8]
    mov r3, #0x04000000
    ldr r0, [r3, #0x210]
    str r0, [r2, #12]
    ldr r0, [r3, #0x214]
    str r0, [r2, #16]
    ldr r0, [r3, #0x208]
    ldr r1, traceSCFG
    ldrh r1, [r1]
    orr r0, r0, r1, lsl #16
    str r0, [r2, #4]
    add r12, r3, #0x100
    ldrh r0, [r12, #0x80]
    str r0, [r2, #20]
    ldrh r0, [r12, #0x84]
    str r0, [r2, #24]
    ldr r0, [r3, #0x1A4]
    str r0, [r2, #28]
    bl tracePublish
    pop {r0-r3, r12, lr}
    stmdb sp!, {lr}
    ldr pc, traceBlobOriginalIRQ

traceBlobCommand:
    push {r0-r3, r12, lr}
    ldr r2, traceMailbox
    str r0, [r2, #32]
    str r1, [r2, #36]
    str lr, [r2, #52]
    bl tracePublish
    pop {r0-r3, r12, lr}
    ldr r3, traceROMCTRL
    ldr pc, traceCommandReturn

traceBlobOverlay:
    push {r0-r3, r12, lr}
    ldr r2, traceMailbox
    ldr r0, [r5]
    str r0, [r2, #44]
    cmp r0, #0x7a
    ldr r0, [r5, #4]
    str r0, [r2, #48]
    streq r0, [r2, #40]
    bl tracePublish
    pop {r0-r3, r12, lr}
    ldr pc, traceOverlayReturn

tracePublish:
    ldr r0, [r2]
    add r0, r0, #1
    str r0, [r2]
    mcr p15, 0, r2, c7, c10, 1
    add r2, r2, #32
    mcr p15, 0, r2, c7, c10, 1
    mcr p15, 0, r2, c7, c10, 4
    bx lr

traceMailbox: .word 0x02000200
traceROMCTRL: .word 0x040001A4
traceSCFG: .word 0x04004000
traceCommandReturn: .word 0x020DCADC
traceOverlayReturn: .word 0x020D210C
traceBlobOriginalIRQ: .word 0
traceBlobEnd:
