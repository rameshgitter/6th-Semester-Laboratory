; HELLOM.asm - Prints "Hello, Ramesh!" using BIOS interrupt 10h

org 100h        ; .COM programs start at offset 100h

section .text

start:
    mov ah, 0Eh     ; BIOS teletype function

    ; Print "Hello, "
    mov al, 'H'
    int 10h
    mov al, 'e'
    int 10h
    mov al, 'l'
    int 10h
    int 10h
    mov al, 'o'
    int 10h
    mov al, ','
    int 10h
    mov al, ' '
    int 10h

    ; Print "Ramesh"
    mov al, 'R'
    int 10h
    mov al, 'a'
    int 10h
    mov al, 'm'
    int 10h
    mov al, 'e'
    int 10h
    mov al, 's'
    int 10h
    mov al, 'h'
    int 10h

    ; Print "!"
    mov al, '!'
    int 10h

    jmp $          ; Infinite loop (hang)

section .data
; (No data section needed for this simple program)
