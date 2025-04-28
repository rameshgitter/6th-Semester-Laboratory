; ... (rest of your code) ...

; --- Bubble Sort Algorithm (Loaded by MBR from Sector 3) ---
section .section2

    bubble_sort:
        push cx             ; Save original array size
        dec cx              ; Decrement CX for outer loop (size - 1)

        outer_loop:
            push cx         ; Save outer loop counter
            mov si, array   ; Reset array pointer
            mov bx, cx      ; Inner loop counter = outer loop counter

        inner_loop:
            mov ax, [si]    ; Load element 1
            mov dx, [si+2]  ; Load element 2
            cmp ax, dx      ; Compare elements
            jle no_swap     ; If element 1 <= element 2, no swap needed

            ; Swap elements
            mov [si], dx    ; Store element 2 in element 1's position
            mov [si+2], ax  ; Store element 1 in element 2's position

        no_swap:
            add si, 2       ; Move to the next pair
            dec bx          ; Decrement inner loop counter
            jnz inner_loop  ; Repeat inner loop if not zero

            pop cx          ; Restore outer loop counter
            loop outer_loop ; Decrement CX and repeat outer loop if not zero

        pop cx              ; Restore original array size
        ret                 ; Return from subroutine