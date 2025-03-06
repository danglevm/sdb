.global main

# global variables, including strings
.section .data

# encodes the string you give it into ASCII with NULL terminator
# %#x is printf format string for hex decimal integers

hex_format:         .asciz "%#x"
float_format:       .asciz "%.2f"
long_float_format:  .asciz "%.2Lf"

.section .text # where code goes


# Trapping
.macro trap  
    movq $62, %rax #syscall id for kill is 62, sends a signal
    movq %r12, %rdi #1st arg is process to be killed off 
    movq $5, %rsi #signal ID, SIGTRAP id of 5
    syscall 
.endm

# get the pid, send a SIGTRAP to the process, read the registers then send it out 

main: # function prologue, carrying out setup - initializing the stack frame
    push    %rbp #store callee-saved reg onto stack and retrieve it
    movq    %rsp, %rbp

    # Get pid syscall
    movq $39, %rax    #39 is getpid syscall
    syscall           #return value stored in rax
    movq %rax, %r12

    trap # 1st

    # print contents of gpr
    # cauculates the address of hex_format string and stores effective address into %rdi regsiter relative to current instruction pointer
    leaq hex_format(%rip), %rdi # calculate an effective address from hex and load effective address into %rdi register
    movq $0, %rax # no vector registers 
    call printf@plt # call functions defined in the shared libraries
    movq $0, %rdi
    call fflush@plt # flush all streams
    trap #2nd

    # mm registers 
    movq %mm0, %rsi
    leaq hex_format(%rip), %rdi
    movq $0, %rax 
    call printf@plt 
    movq $0, %rdi 
    call fflush@plt 
    trap  #3rd


    # print xmm0 register 
    leaq float_format(%rip), %rdi
    movq $1, %rax   # one fp argument
    call printf@plt 
    movq $0, %rdi  
    call fflush@plt 
    trap  #4th

    # print contents of st0
    subq  $16, %rsp # allocates 16B on the stack  
    fstpt (%rsp)    # pop st0 from the top of the FPU stack
    leaq  long_float_format(%rip), %rdi 
    movq  $0, %rax   # no fp arguments
    call  printf@plt 
    movq  $0, %rdi 
    call  fflush@plt 
    addq  $16, %rsp # give back 16 bytes of stack space
    trap   #5th

    popq    %rbp # function epilogue carrying out cleanup
    movq    $0, %rax 
    ret