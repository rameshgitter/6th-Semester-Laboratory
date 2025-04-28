org 800h

continue_sort:
    pop cx              ; Restore outer loop counter
    dec cx              ; Decrement counter
    jz display          ; If zero, we're done sorting
    push cx             ; Save counter again
    mov si, array       ; Reset array pointer
    jmp 0600h           ; Return to outer loop
    
display:
    ; Print newline
    mov ah, 0Eh
    mov al, 13
    int 10h
    mov al, 10
    int 10h
    
    ; Display sorted array
    mov cx, 9           ; Array length
    mov si, array       ; Array pointer
print_array:
    mov al, [si]        ; Get number
    add al, '0'         ; Convert to ASCII
    mov ah, 0Eh         ; BIOS teletype
    int 10h
    
    mov al, ' '         ; Space between numbers
    int 10h
    
    inc si
    loop print_array
    
    ; Print newline
    mov al, 13
    int 10h
    mov al, 10
    int 10h
    
    hlt                 ; Stop execution

array db 9, 5, 1, 4, 3, 7, 6, 8, 2  ; Array to sort

times 510-($-$$) db 0
