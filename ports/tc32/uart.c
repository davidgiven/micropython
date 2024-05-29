#include "port.h"

#include "py/builtin.h"
#include "py/compile.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/repl.h"
#include "py/runtime.h"
#include "shared/runtime/pyexec.h"

static uint8_t recvbuf[16];

void uart_init(void) {
  uart_set_recbuff((unsigned short *)recvbuf, sizeof(recvbuf));
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

mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
  while (len--)
    uart_ndma_send_byte(*str++);
  return 0;
}
