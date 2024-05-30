#include "port.h"

#include "py/builtin.h"
#include "py/compile.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/repl.h"
#include "py/runtime.h"
#include "shared/runtime/pyexec.h"

#define PIN GPIO_PC1

_attribute_ram_code_ void irq_handler(void) {}

extern uint8_t _end_bss_;

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

mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
  mp_raise_OSError(MP_ENOENT);
}

mp_import_stat_t mp_import_stat(const char *path) {
  return MP_IMPORT_STAT_NO_EXIST;
}

mp_obj_t mp_builtin_open(size_t n_args, const mp_obj_t *args,
                         mp_map_t *kwargs) {
  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, mp_builtin_open);

void NORETURN __fatal_error(const char *msg) {
  mp_hal_stdout_tx_strn(msg, strlen(msg));
  while (1) {
    ;
  }
}

void nlr_jump_fail(void *val) { __fatal_error("NLR jump fail"); }

#if !MICROPY_DEBUG_PRINTERS
// With MICROPY_DEBUG_PRINTERS disabled DEBUG_printf is not defined but it
// is still needed by esp-open-lwip for debugging output, so define it here.
#include <stdarg.h>
int mp_vprintf(const mp_print_t *print, const char *fmt, va_list args);
int DEBUG_printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = mp_vprintf(MICROPY_DEBUG_PRINTER, fmt, ap);
  va_end(ap);
  return ret;
}
#endif

int main() {
  cpu_wakeup_init();
  clock_init(SYS_CLK_16M_Crystal);
  gpio_init();

  uart_init();

  gpio_set_func(PIN, AS_GPIO);
  gpio_set_output_en(PIN, 1);
  gpio_set_input_en(PIN, 0);
  gpio_write(PIN, 1);

  /* The Telink SDK doesn't define any symbols for these, so we just hard code it. The stack starts at the end of memory, 0x80c000. We leave 1kB for it,
   * meaning that our heap needs to end at 0x80bc00. */
  gc_init((void *)&_end_bss_, (void *)0x80bc00);
  mp_init();
  pyexec_friendly_repl();
  mp_deinit();
  return 0;
}
