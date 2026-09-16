.section .text.start, "ax", @progbits
.globl _start
.type _start, @function
_start:
  .option push
  .option norelax
  la sp, __stack_top
  la gp, __global_pointer$
  .option pop
  la t0, __bss_start
  la t1, __bss_end
.Lclear:
  bgeu t0, t1, .Lmain
  sw zero, 0(t0)
  addi t0, t0, 4
  jal zero, .Lclear
.Lmain:
  call main
  addi a7, zero, 93
  ecall
.Lstopped:
  jal zero, .Lstopped
.size _start, .-_start
.section .note.GNU-stack, "", @progbits
