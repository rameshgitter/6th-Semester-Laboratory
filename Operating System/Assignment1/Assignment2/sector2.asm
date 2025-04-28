org 100h
section .text
start:
    ; Print "Sorting..." message
    mov si, msg
    mov ah, 0Eh
print:
    lodsb
    test al, al
    jz init
    int 10h
    jmp print

init:
    mov cx, 9          ; Hardcode array length for now
    dec cx             ; Length - 1 for outer loop
    
outer_loop:
    push cx            ; Save outer loop counter
    mov si, 0800h      ; Start of array in sector 4 (array is at beginning)
    jmp 0:0700h        ; Jump to sector 3

msg db 'Sorting...', 13, 10, 0

; Pad to 512 bytes
times 510-($-$$) db 0
