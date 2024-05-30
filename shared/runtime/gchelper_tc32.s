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

    .section .text
    .align  2

    .global gc_helper_get_regs_and_sp
    .type gc_helper_get_regs_and_sp, %function

@ The TC32 is basically a Thumb with marginally different instructions and
@ instruction encodings, so this is copied from the Thumb implementation with
@ some search-and-replace.

@ uint gc_helper_get_regs_and_sp(r0=uint regs[10])
gc_helper_get_regs_and_sp:
    @ store registers into given array
    tstorer r4, [r0, #0]
    tstorer r5, [r0, #4]
    tstorer r6, [r0, #8]
    tstorer r7, [r0, #12]
    tmovs r1, r8
    tstorer r1, [r0, #16]
    tmovs r1, r9
    tstorer r1, [r0, #20]
    tmovs r1, r10
    tstorer r1, [r0, #24]
    tmovs r1, r11
    tstorer r1, [r0, #28]
    tmovs r1, r12
    tstorer r1, [r0, #32]
    tmovs r1, r13
    tstorer r1, [r0, #36]

    @ return the sp
    tmov r0, sp
    tjex lr

    .size gc_helper_get_regs_and_sp, .-gc_helper_get_regs_and_sp
