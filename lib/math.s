/*
r0: dividend
r1: divisor
  returns
r0: result
*/
  .globl divide_u32
  .globl __aeabi_uidiv
  .globl __aeabi_uidivmod
divide_u32:
__aeabi_uidiv:
__aeabi_uidivmod:
  result .req r0
  remainder .req r1
  shift .req r2
  current .req r3

  clz shift, r1
  clz r3, r0
  subs shift, r3
  lsl current, r1, shift
  mov remainder, r0
  mov result, #0
  blt $divide_u32_return

$divide_u32_loop:
  cmp remainder, current
  blt $divide_u32_loop_continue

  add result, result, #1
  subs remainder, current // short circuit if we evenly divided
  lsleq result, shift
  beq $divide_u32_return

$divide_u32_loop_continue:
  subs shift, #1
  lsrge current, #1
  lslge result, #1
  bge $divide_u32_loop

$divide_u32_return:
  .unreq current
  mov pc, lr
  .unreq result
  .unreq remainder
  .unreq shift
