/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2023 Damien P. George
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

#include <stdio.h>
#include <string.h>

#include "port.h"

#include "extmod/modmachine.h"
#include "extmod/virtpin.h"
#include "machine_pin.h"
#include "modmachine.h"
#include "mphalport.h"
#include "py/mphal.h"
#include "py/runtime.h"

enum { PINMODE_IN, PINMODE_OUT, PINMODE_ALT };

const machine_pin_obj_t *machine_pin_find_named(const mp_obj_dict_t *named_pins,
                                                mp_obj_t name) {
  const mp_map_t *named_map = &named_pins->map;
  mp_map_elem_t *named_elem =
      mp_map_lookup((mp_map_t *)named_map, name, MP_MAP_LOOKUP);
  if (named_elem != NULL && named_elem->value != NULL) {
    return MP_OBJ_TO_PTR(named_elem->value);
  }
  return NULL;
}

void machine_pins_init(void) {}

void machine_pins_deinit(void) {}

const machine_pin_obj_t *machine_pin_find(mp_obj_t pin_in) {
  if (mp_obj_is_type(pin_in, &machine_pin_type)) {
    return pin_in;
  }

  if (mp_obj_is_str(pin_in)) {
    // Try to find the pin in the board pins dict.
    const machine_pin_obj_t *self =
        machine_pin_find_named(&machine_pin_board_pins_locals_dict, pin_in);
    if (self && self->base.type != NULL)
      return self;

    // Try to find the pin in the MCU pins pict.
    self = machine_pin_find_named(&machine_pin_cpu_pins_locals_dict, pin_in);
    if (self != NULL)
      return self;
  }

  mp_raise_ValueError(MP_ERROR_TEXT("invalid pin"));
}

static void machine_pin_print(const mp_print_t *print, mp_obj_t self_in,
                              mp_print_kind_t kind) {
  machine_pin_obj_t *self = self_in;
  mp_printf(print, "Pin(%q)", self->name);
}

// pin.init(mode=None, pull=-1, *, value, drive, hold)
static mp_obj_t machine_pin_obj_init_helper(const machine_pin_obj_t *self,
                                            size_t n_args,
                                            const mp_obj_t *pos_args,
                                            mp_map_t *kw_args) {
  enum { ARG_mode, ARG_pull, ARG_value };
  static const mp_arg_t allowed_args[] = {
      {MP_QSTR_mode, MP_ARG_OBJ, {.u_obj = mp_const_none}},
      {MP_QSTR_pull, MP_ARG_OBJ, {.u_obj = MP_OBJ_NEW_SMALL_INT(-1)}},
      {MP_QSTR_value, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none}},
  };

  /* Parse arguments. */

  mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
  mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args),
                   allowed_args, args);

  /* Get initial value for the pin. */

  int value = -1;
  if (args[ARG_value].u_obj != mp_const_none) {
    value = mp_obj_is_true(args[ARG_value].u_obj);
  }

  /* Pin mode. */

  if (args[ARG_mode].u_obj != mp_const_none) {
    int mode = mp_obj_get_int(args[ARG_mode].u_obj);

    if (mode == PINMODE_ALT) {
      mp_raise_ValueError(MP_ERROR_TEXT("not supported yet"));
    }

    gpio_set_func(self->gpio, AS_GPIO);
    if (value != -1)
      gpio_write(self->gpio, value);
    gpio_set_input_en(self->gpio, mode == PINMODE_IN);
    gpio_set_output_en(self->gpio, mode == PINMODE_OUT);
  }

  /* Pull up/down. */

  if (args[ARG_pull].u_obj == mp_const_none) {
    gpio_setup_up_down_resistor(self->gpio, GPIO_PULL_UP_DOWN_FLOAT);
  } else if (args[ARG_pull].u_obj != MP_OBJ_NEW_SMALL_INT(-1)) {
    GPIO_PullTypeDef mode = mp_obj_get_int(args[ARG_pull].u_obj);
    switch (mode) {
    case GPIO_PULL_UP_1M:
    case GPIO_PULL_UP_10K:
    case GPIO_PULL_DOWN_100K:
      gpio_setup_up_down_resistor(self->gpio, mode);
      break;

    default:
      mp_raise_ValueError(MP_ERROR_TEXT("invalid pullup"));
    }
  }

  return mp_const_none;
}

// constructor(id, ...)
mp_obj_t mp_pin_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw,
                         const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 1, MP_OBJ_FUN_ARGS_MAX, true);

  // get the wanted pin object
  const machine_pin_obj_t *self = machine_pin_find(args[0]);

  if (n_args > 1 || n_kw > 0) {
    // pin mode given, so configure this GPIO
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    machine_pin_obj_init_helper(self, n_args - 1, args + 1, &kw_args);
  }

  return MP_OBJ_FROM_PTR(self);
}

// fast method for getting/setting pin value
static mp_obj_t machine_pin_call(mp_obj_t self_in, size_t n_args, size_t n_kw,
                                 const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 0, 1, false);
  machine_pin_obj_t *self = self_in;
  if (n_args == 0) {
    /* Get value. */
    return MP_OBJ_NEW_SMALL_INT(gpio_read(self->gpio));
  } else {
    /* Set value. */
    gpio_write(self->gpio, mp_obj_is_true(args[0]));
    return mp_const_none;
  }
}

// pin.init(mode, pull)
static mp_obj_t machine_pin_obj_init(size_t n_args, const mp_obj_t *args,
                                     mp_map_t *kw_args) {
  return machine_pin_obj_init_helper(args[0], n_args - 1, args + 1, kw_args);
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_pin_init_obj, 1, machine_pin_obj_init);

// pin.value([value])
static mp_obj_t machine_pin_value(size_t n_args, const mp_obj_t *args) {
  return machine_pin_call(args[0], n_args - 1, 0, args + 1);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_pin_value_obj, 1, 2,
                                           machine_pin_value);

// pin.off()
static mp_obj_t machine_pin_off(mp_obj_t self_in) {
  machine_pin_obj_t *self = MP_OBJ_TO_PTR(self_in);
  gpio_write(self->gpio, true);
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_pin_off_obj, machine_pin_off);

// pin.on()
static mp_obj_t machine_pin_on(mp_obj_t self_in) {
  machine_pin_obj_t *self = MP_OBJ_TO_PTR(self_in);
  gpio_write(self->gpio, false);
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_pin_on_obj, machine_pin_on);

MP_DEFINE_CONST_OBJ_TYPE(machine_pin_board_pins_obj_type, MP_QSTR_board,
                         MP_TYPE_FLAG_NONE, locals_dict,
                         &machine_pin_board_pins_locals_dict);

static const mp_rom_map_elem_t machine_pin_locals_dict_table[] = {
    // instance methods
    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_pin_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_value), MP_ROM_PTR(&machine_pin_value_obj)},
    {MP_ROM_QSTR(MP_QSTR_off), MP_ROM_PTR(&machine_pin_off_obj)},
    {MP_ROM_QSTR(MP_QSTR_on), MP_ROM_PTR(&machine_pin_on_obj)},

    // class attributes
    {MP_ROM_QSTR(MP_QSTR_board), MP_ROM_PTR(&machine_pin_board_pins_obj_type)},

    // class constants
    {MP_ROM_QSTR(MP_QSTR_IN), MP_ROM_INT(PINMODE_IN)},
    {MP_ROM_QSTR(MP_QSTR_OUT), MP_ROM_INT(PINMODE_OUT)},
    {MP_ROM_QSTR(MP_QSTR_ALT), MP_ROM_INT(PINMODE_ALT)},

    {MP_ROM_QSTR(MP_QSTR_PULL_UP), MP_ROM_INT(GPIO_PULL_UP_10K)},
    {MP_ROM_QSTR(MP_QSTR_PULL_UP_10K), MP_ROM_INT(GPIO_PULL_UP_10K)},
    {MP_ROM_QSTR(MP_QSTR_PULL_DOWN), MP_ROM_INT(GPIO_PULL_DOWN_100K)},
};

static mp_uint_t pin_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg,
                           int *errcode) {
  (void)errcode;
  machine_pin_obj_t *self = self_in;

  switch (request) {
  case MP_PIN_READ:
    return gpio_read(self->gpio);

  case MP_PIN_WRITE: {
    gpio_write(self->gpio, arg);
    return 0;
  }
  }
  return -1;
}

static MP_DEFINE_CONST_DICT(machine_pin_locals_dict,
                            machine_pin_locals_dict_table);

static const mp_pin_p_t pin_pin_p = {
    .ioctl = pin_ioctl,
};

MP_DEFINE_CONST_OBJ_TYPE(machine_pin_type, MP_QSTR_Pin, MP_TYPE_FLAG_NONE,
                         make_new, mp_pin_make_new, print, machine_pin_print,
                         call, machine_pin_call, protocol, &pin_pin_p,
                         locals_dict, &machine_pin_locals_dict);
