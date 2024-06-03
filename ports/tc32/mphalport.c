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
