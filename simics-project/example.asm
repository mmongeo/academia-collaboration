ORG 0x0                 ; add to offsets

start:  
    mov eax, 0          ; primer número de fibonacci
    mov ebx, 1          ; segundo número de fibonacci
    mov ecx, 10         ; número de términos a calcular

    fibonacci:          ; ciclo para calcular los términos de Fibonacci
        mov edx, eax
        mov eax, ebx
        add eax, edx    ; numeros de fibonacci ciclo a ciclo (observable en Simics Control Menu -> Debug -> CPU Registers)
        mov ebx, edx    
        dec ecx
        jnz fibonacci

section .data
    fib: db 0, 1, 1, 2, 3, 5, 8, 13, 21, 34