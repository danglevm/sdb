.global main

# global variables, including strings
.section .data

# encodes the string you give it into ASCII with NULL terminator
# %#x is printf format string for hex decimal integers
hex_format: .asciz "%#x"

.section .text # where code goes


# Trapping
.macro trap  
    movq $62, %rax
    movq %r12, %rdi 
    movq $5, %rsi 
    syscall 
.endm

main: # function prologue, carrying out setup - initializing the stack frame
    push    %rbp
    movq    %rsp, %rbp

    # Get pid syscall
    movq $39, %rax
    syscall
    movq %rax, %r12

    # print contents of rsi 

    # cauculates the address of hex_format string and stores effective address into %rdi regsiter relative to current instruction pointer
    leaq hex_format(%rip), %rdi # calculate an effective address from hex and load effective address into %rdi register
    movq $0, %rax # no vector registers 
    call printf@plt # call functions defined in the shared libraries
    movq $0, %rdi
    call fflush@plt # flush all streams

    trap

    popq    %rbp # function epilogue carrying out cleanup
    movq    $0, %rax 
    ret