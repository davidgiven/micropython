/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2024 Damien P. George, David Given
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

#include "port.h"
#include "py/mphal.h"
#include "py/stream.h"

void mp_hal_set_interrupt_char(char c) {}

void mp_hal_delay_us(uint32_t delay) { sleep_us(delay); }

void mp_hal_delay_ms(uint32_t delay) { sleep_us(delay * 1000); }

uint32_t mp_hal_ticks_cpu() { return clock_time(); }

uint32_t mp_hal_ticks_us() {
  return clock_time() / CLOCK_16M_SYS_TIMER_CLK_1US;
}

uint32_t mp_hal_ticks_ms() {
  return clock_time() / CLOCK_16M_SYS_TIMER_CLK_1MS;
}

void uart_init(void) {
  uart_set_pin(GPIO_PB4, GPIO_PB5);
  uart_reset();
  uart_init_baudrate(9, 13, PARITY_NONE, STOP_BIT_ONE); /* 115200 baud */

  uart_irq_en(true, false);      /* UART generates interrupt when... */
  uart_ndma_set_triglevel(1, 0); /* ...one byte is received */
}

int mp_hal_stdin_rx_chr(void) {
  /* Weirdly, the 8232 driver has no read routines. */

  /* Block until a character is available. */
  while (!uart_ndma_get_irq())
    ;

  /* Read the character from the rotating buffer (this will automatically clear
   * the IRQ). */
  static uint8_t index = 0;
  uint8_t value = reg_uart_data_buf(index);
  index++;
  index &= 0x03;

  return value;
}

mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len) {
  while (len--)
    uart_ndma_send_byte(*str++);
  return 0;
}

uintptr_t mp_hal_stdio_poll(uintptr_t poll_flags) {
  uintptr_t ret = 0;

  if ((poll_flags & MP_STREAM_POLL_RD) && !uart_ndma_get_irq())
    ret |= MP_STREAM_POLL_RD;
  if ((poll_flags & MP_STREAM_POLL_WR) && !uart_tx_is_busy())
    ret |= MP_STREAM_POLL_WR;

  return ret;
}
