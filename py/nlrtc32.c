/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/mpstate.h"

#if MICROPY_NLR_TC32

#undef nlr_push

// We only need the functions here if we are on TC32, and we are not
// using setjmp/longjmp.
//
// For reference, TC32 callee save regs are the same as on Thumb:
//      r4-r11, r13=sp

__attribute__((naked)) unsigned int nlr_push(nlr_buf_t *nlr) {

  // If you get a linker error here, indicating that a relocation doesn't
  // fit, try the following (in that order):
  //
  // 1. Ensure that nlr.o nlrthumb.o are linked closely together, i.e.
  //    there aren't too many other files between them in the linker list
  //    (PY_CORE_O_BASENAME in py/py.mk)
  // 2. Set -DMICROPY_NLR_THUMB_USE_LONG_JUMP=1 during the build
  //
  __asm volatile("tstorer r4, [r0, #12]       \n" // store r4 into nlr_buf
                 "tstorer r5, [r0, #16]       \n" // store r5 into nlr_buf
                 "tstorer r6, [r0, #20]       \n" // store r6 into nlr_buf
                 "tstorer r7, [r0, #24]       \n" // store r7 into nlr_buf
                 "tmov    r1, r8              \n"
                 "tstorer r1, [r0, #28]       \n" // store r8 into nlr_buf
                 "tmov    r1, r9              \n"
                 "tstorer r1, [r0, #32]       \n" // store r9 into nlr_buf
                 "tmov    r1, r10             \n"
                 "tstorer r1, [r0, #36]       \n" // store r10 into nlr_buf
                 "tmov    r1, r11             \n"
                 "tstorer r1, [r0, #40]       \n" // store r11 into nlr_buf
                 "tmov    r1, r13             \n"
                 "tstorer r1, [r0, #44]       \n" // store r13=sp into nlr_buf
                 "tmov    r1, lr              \n"
                 "tstorer r1, [r0, #8]        \n" // store lr into nlr_buf

                 "tj      nlr_push_tail       \n" // do the rest in C
  );

#if !defined(__clang__) && defined(__GNUC__) &&                                \
    (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8))
  // Older versions of gcc give an error when naked functions don't return a
  // value Additionally exclude Clang as it also defines __GNUC__ but doesn't
  // need this statement
  return 0;
#endif
}

NORETURN void nlr_jump(void *val) {
  MP_NLR_JUMP_HEAD(val, top)

  __asm volatile("tmov    r0, %0              \n" // r0 points to nlr_buf
                 "tloadr  r4, [r0, #12]       \n" // load r4 from nlr_buf
                 "tloadr  r5, [r0, #16]       \n" // load r5 from nlr_buf
                 "tloadr  r6, [r0, #20]       \n" // load r6 from nlr_buf
                 "tloadr  r7, [r0, #24]       \n" // load r7 from nlr_buf

                 "tloadr  r1, [r0, #28]       \n" // load r8 from nlr_buf
                 "tmov    r8, r1              \n"
                 "tloadr  r1, [r0, #32]       \n" // load r9 from nlr_buf
                 "tmov    r9, r1              \n"
                 "tloadr  r1, [r0, #36]       \n" // load r10 from nlr_buf
                 "tmov    r10, r1             \n"
                 "tloadr  r1, [r0, #40]       \n" // load r11 from nlr_buf
                 "tmov    r11, r1             \n"
                 "tloadr  r1, [r0, #44]       \n" // load r13=sp from nlr_buf
                 "tmov    r13, r1             \n"
                 "tloadr  r1, [r0, #8]        \n" // load lr from nlr_buf
                 "tmov    lr, r1              \n"
                 "tmovs   r0, #1              \n" // return 1, non-local return
                 "tjex    lr                  \n" // return
                 :                                // output operands
                 : "r"(top)                       // input operands
                 : "memory"                       // clobbered registers
  );

  MP_UNREACHABLE
}

#endif // MICROPY_NLR_THUMB
