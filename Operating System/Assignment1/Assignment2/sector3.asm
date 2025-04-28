org 700h

inner_loop:
    mov al, [si]       ; Get current element
    mov bl, [si+1]     ; Get next element
    cmp al, bl         ; Compare them
    jle no_swap        ; If in order, don't swap
    
    ; Swap elements
    mov [si], bl
    mov [si+1], al
    
no_swap:
    inc si             ; Move to next element
    loop inner_loop    ; Continue inner loop
    
    jmp 0:0800h        ; Jump to sector 4

times 510-($-$$) db 0
