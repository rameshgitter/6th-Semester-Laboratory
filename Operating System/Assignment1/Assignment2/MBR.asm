org 0x7c00

start:
    ; Initialize segment registers and stack
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    ; Reset disk system
    call reset_disk
    jc reset_error ; Jump if reset fails

    ; Load required sectors
    call load_sector    ; Load sector 2 (initialization)
    jc read_error

    mov bx, 0x0700
    mov cl, 0x03       ; Sector 3
    call load_sector    ; Load sector 3 (sort logic)
    jc read_error

    mov bx, 0x0800
    mov cl, 0x04       ; Sector 4
    call load_sector    ; Load sector 4 (data and display)
    jc read_error

    ; Jump to the start of the loaded program
    jmp 0x0000:0x0600

reset_error:
    mov si, reset_msg
    jmp print_error

read_error:
    mov si, read_msg
    jmp print_error

; Subroutine: Reset Disk System
reset_disk:
    xor ah, ah         ; Reset disk system
    int 0x13
    ret

; Subroutine: Load Sector
load_sector:
    mov ah, 0x02       ; Read sectors
    mov al, 0x01       ; One sector
    mov ch, 0x00       ; Cylinder 0
    mov dh, 0x00       ; Head 0
    mov dl, 0x00       ; Drive 0 (First drive)
    int 0x13
    ret

; Subroutine: Print Error Message
print_error:
    mov ah, 0x0E       ; BIOS teletype output
print_loop:
    lodsb              ; Load next character from SI
    test al, al        ; Check if end of string (null terminator)
    jz hang            ; Halt if end of string
    int 0x10           ; Print character
    jmp print_loop

hang:
    hlt                ; Halt CPU
    jmp hang           ; Infinite loop

; Error messages
reset_msg db 'Disk reset failed!', 0
read_msg db 'Read error occurred!', 0

; Padding and boot signature
times 510-($-$$) db 0
dw 0xAA55

