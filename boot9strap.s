.create "build/code11.bin",code_11_load_addr
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; boot11_hook: This code is called by boot11 just before lockout.
;              It copies the bootrom to axi_wram, then syncs with
;              boot9 hook.
boot11_hook:
    mov r11, r0

    ldr r1, =b11_axi_addr      
    ldr r0, =0x10000            
    mov r2, #0x0
    b11_copy_loop:           ; Simple memcpy loop from boot11 to axiwram.
        ldr r3, [r0, r2]
        str r3, [r1, r2]
        add r2, r2, #0x4
        cmp r2, r0
        blt b11_copy_loop

    ldr r1, =b11_axi_addr    ; Let boot9 know that we are done.
    mov r0, #0x1
    str r0, [r1, #-0x4]

    wait_for_b9_copy:        ; Wait for boot9 to confirm it received our dump.
        ldr r0, [r1, #-0x4]
        cmp r0, #0x0
        bne wait_for_b9_copy

    bx r11                   ; Jump to entrypoint

.pool

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; arm11 stage 2
.org (code_11_load_addr+0x200)

; this only runs on core0

.area 0x10000
.incbin "stage2/arm11/arm11.bin"
.endarea
.align 0x200

.close

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; NDMA section: This generates the NDMA overwrite file.

.create "build/NDMA.bin",0
.area 0x200
.dw 0x00000000          ; NDMA Global CNT
.dw dabrt_vector        ; Source Address
.dw arm9mem_dabrt_loc   ; Destination Address
.dw 0x00000000          ; Unused Total Repeat Length
.dw 0x00000002          ; Transfer 2 words
.dw 0x00000000          ; Transfer until completed
.dw 0x00000000          ; Unused Fill Data
.dw 0x90010000          ; Start Immediately/Transfer 2 words at a time/Enable
.endarea
.align 0x200
.close

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Data abort section: This is just a single sector causes boot9 to data abort.

.create "build/dabrt.bin",0
.area 0x200, 0xFF
.endarea
.close
