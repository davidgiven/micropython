#include "port.h"

#include "extmod/modmachine.h"
#include "extmod/vfs.h"
#include "lib/littlefs/lfs1.h"
#include "lib/littlefs/lfs1_util.h"
#include "machine_pin.h"
#include "modmachine.h"
#include "py/builtin.h"
#include "py/compile.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/repl.h"
#include "py/runtime.h"
#include "shared/runtime/pyexec.h"

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

MP_NOINLINE static void hard_init() {
  cpu_wakeup_init();
  clock_init(SYS_CLK_16M_Crystal);
  gpio_init();
  uart_init();
}

MP_NOINLINE static void soft_init() {

  /* The Telink SDK doesn't define any symbols for these, so we just hard code
   * it. The stack starts at the end of memory, 0x80c000. We leave 1kB for it,
   * meaning that our heap needs to end at 0x80bc00. */
  gc_init((void *)&_end_bss_, (void *)0x80bc00);

  /* Set up the stack corruption flag byte. */
  *(uint8_t*)0x80bc00 = 42;

  mp_init();
  machine_init();
}

MP_NOINLINE static void repl() {
  pyexec_frozen_module("_boot.py", false);
  int ret = pyexec_file_if_exists("boot.py");
  if (ret)
    mp_printf(&mp_plat_print, "mpy: couldn't execute boot.py\n");

  for (;;) {
    if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
      if (pyexec_raw_repl() != 0) {
        return;
      }
    } else {
      if (pyexec_friendly_repl() != 0) {
        return;
      }
    }
  }
}

MP_NOINLINE static void soft_deinit() {
  mp_printf(&mp_plat_print, "vfs: sync filesystems\n");
  machine_deinit();
  gc_sweep_all();
  mp_deinit();
}

// We should capture stack top ASAP after start, and it should be captured
// guaranteedly before any other stack variables are allocated.  For this, the
// functions being called here shouldn't be inlined into the nmain function, as
// they may have stack variables.
int main() {
  hard_init();
  for (;;) {
    soft_init();

    // Capture the stack.
    mp_stack_ctrl_init();

    repl();

    mp_printf(&mp_plat_print, "mpy: soft reboot\n");
    soft_deinit();
  }
  return 0;
}
