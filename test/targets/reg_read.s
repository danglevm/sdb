.global main

.section .data
my_double: .double 64.125

.section .text

# Trapping
.macro trap  
    movq $62, %rax
    movq %r12, %rdi 
    movq $5, %rsi 
    syscall 
.endm


main: 
    push %rbp 
    movq %rsp, %rbp 

    # Get PID syscall 
    movq $39, %rax 
    syscall 
    movq %rax, %r12 
    

    # try to read 0xcafecafe from r13 register
    # store to r13 
    movq $0xcafecafe, %r13 
    trap 

    # store to r13b 
    movb $42, %r13b  # test sub-registers r13b
    trap

    # store to mm0
    # can't move directly from 64-bit immediate to MMX reg, store to r13 then to mm0
    movq $0xba5eba11, %r13 
    movq %r13, %mm0 
    trap 

    # store to xmm0
    movsd my_double(%rip), %xmm0
    trap 

    # store to st0
    emms 
    fldl my_double(%rip) # load floating point value
    trap 

    popq %rbp 
    movq $0, %rax
    ret