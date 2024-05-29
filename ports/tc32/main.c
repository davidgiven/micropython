#include "drivers/5316/bsp.h"
#include "drivers/5316/clock.h"
#include "drivers/5316/compiler.h"
#include "drivers/5316/driver_5316.h"
#include "drivers/5316/gpio.h"
#include "drivers/5316/timer.h"
#include "drivers/5316/uart.h"

#undef __SIZE_TYPE__
#define __SIZE_TYPE__ u32
#undef __WCHAR_TYPE__
#define __WCHAR_TYPE__ u16

#include "py/builtin.h"
#include "py/compile.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/repl.h"
#include "py/runtime.h"
#include "shared/runtime/pyexec.h"

#define PIN GPIO_PC1

_attribute_ram_code_ void irq_handler(void) {}

static char *stack_top;
static char heap[MICROPY_HEAP_SIZE];

void do_str(const char *src, mp_parse_input_kind_t input_kind) {
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_lexer_t *lex =
        mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, src, strlen(src), 0);
    qstr source_name = lex->source_name;
    mp_parse_tree_t parse_tree = mp_parse(lex, input_kind);
    mp_obj_t module_fun = mp_compile(&parse_tree, source_name, true);
    mp_call_function_0(module_fun);
    nlr_pop();
  } else {
    // uncaught exception
    mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
  }
}

int mp_hal_stdin_rx_chr(void) { return -1; }

mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
  while (len--)
    uart_ndma_send_byte(*str++);
  return 0;
}

mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
  mp_raise_OSError(MP_ENOENT);
}

mp_import_stat_t mp_import_stat(const char *path) {
  return MP_IMPORT_STAT_NO_EXIST;
}

void nlr_jump_fail(void *val) {
  while (1) {
    ;
  }
}

void NORETURN __fatal_error(const char *msg) {
  while (1) {
    ;
  }
}

void gc_collect(void) {
  // WARNING: This gc_collect implementation doesn't try to get root
  // pointers from CPU registers, and thus may function incorrectly.
  void *dummy;
  gc_collect_start();
  gc_collect_root(&dummy, ((mp_uint_t)stack_top - (mp_uint_t)&dummy) /
                              sizeof(mp_uint_t));
  gc_collect_end();
  gc_dump_info(&mp_plat_print);
}

int main() {
  cpu_wakeup_init();
  clock_init(SYS_CLK_16M_Crystal);
  gpio_init();

  uart_set_pin(GPIO_PB4, GPIO_PB5);
  uart_init_baudrate(9, 13, PARITY_NONE, STOP_BIT_ONE);

  gpio_set_func(PIN, AS_GPIO);
  gpio_set_output_en(PIN, 1);
  gpio_set_input_en(PIN, 0);
  gpio_write(PIN, 1);

  int stack_dummy;
  stack_top = (char *)&stack_dummy;

  gc_init(heap, heap + sizeof(heap));
  mp_init();
  pyexec_friendly_repl();
  mp_deinit();
  return 0;
}
