ORG 0x0fffffff0           ; add to offsets

start:
   mov al, 0   ; primer número de Fibonacci
   mov bl, 1   ; segundo número de Fibonacci
   mov cl, 5   ; número de términos a calcular
   ; Ciclo para calcular los términos de Fibonacci
   fibonacci:
      add al, bl
      mov bl, al
      sub al, bl
      dec cl
      jnz fibonacci

section .data
   fib: db 0, 1, 1, 2, 3, 5, 8, 13, 21, 34