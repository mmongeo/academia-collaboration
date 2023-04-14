ORG 0x0fffffff0           ; Offset

start:
   ;mov ax, 0x0000 
   ;mov ds, ax
   ;mov ax, 0x1000
   ;mov ss, ax

   mov ax, 0   ; primer número de Fibonacci
   mov bx, 1   ; segundo número de Fibonacci
   mov cx, 10   ; número de términos a calcular
   ; Ciclo para calcular los términos de Fibonacci
   fibonacci:
      ; Calcula el siguiente término de Fibonacci
      add ax, bx
      mov dx, bx
      ;mov bx, ax
      ;mov ax, dx 
      dec cx
      jnz fibonacci
    
   ; Termina el programa

section .data
   fib: db 0, 1, 1, 2, 3, 5, 8, 13, 21, 34