/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Damien P. George
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

#include "port.h"

#include "extmod/vfs.h"
#include "modtc32.h"
#include "py/runtime.h"

#include "drivers/5316/flash.h"

typedef struct _tc32_flash_obj_t {
  mp_obj_base_t base;
  uint32_t flash_base;
  uint32_t flash_size;
} tc32_flash_obj_t;

#define SECTOR_SIZE 4096

/* Our singleton flash storage object. */
static tc32_flash_obj_t tc32_flash_obj = {{&tc32_flash_type}};

static mp_obj_t tc32_flash_make_new(const mp_obj_type_t *type, size_t n_args,
                                    size_t n_kw, const mp_obj_t *all_args) {
  // No args required. bdev=Flash(). Start Addr & Size defined in
  // tc32_flash_obj.
  mp_arg_check_num(n_args, n_kw, 0, 0, false);

  uint32_t mid = flash_read_mid();
  Flash_CapacityDef capacity = (mid & 0x00ff0000) >> 16;
  if (capacity != FLASH_SIZE_512K) {
    mp_raise_ValueError(MP_ERROR_TEXT("unsupported flash size"));
  }

  extern uint8_t _bin_size_; /* provided by linker */
  tc32_flash_obj.flash_base =
      ((uint32_t)&_bin_size_ + SECTOR_SIZE - 1) & ~(SECTOR_SIZE - 1);
  tc32_flash_obj.flash_size =
      (CFG_ADR_MAC - tc32_flash_obj.flash_base + SECTOR_SIZE - 1) &
      ~(SECTOR_SIZE - 1);

  // Return singleton object.
  return &tc32_flash_obj;
}

static mp_obj_t tc32_flash_readblocks(size_t n_args, const mp_obj_t *args) {
  uint32_t offset =
      (mp_obj_get_int(args[1]) * SECTOR_SIZE) + tc32_flash_obj.flash_base;
  mp_buffer_info_t bufinfo;
  mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_WRITE);
  if (n_args == 4) {
    offset += mp_obj_get_int(args[3]);
  }

  flash_read_data(offset, bufinfo.len, (uint8_t *)bufinfo.buf);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tc32_flash_readblocks_obj, 3, 4,
                                           tc32_flash_readblocks);

static mp_obj_t tc32_flash_writeblocks(size_t n_args, const mp_obj_t *args) {
  uint32_t offset =
      (mp_obj_get_int(args[1]) * SECTOR_SIZE) + tc32_flash_obj.flash_base;
  mp_buffer_info_t bufinfo;
  mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_READ);
  if (n_args == 3) {
    flash_erase_sector(offset);
  } else {
    offset += mp_obj_get_int(args[3]);
  }

  flash_page_program(offset, bufinfo.len, bufinfo.buf);
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tc32_flash_writeblocks_obj, 3, 4,
                                           tc32_flash_writeblocks);

static mp_obj_t tc32_flash_ioctl(mp_obj_t self_in, mp_obj_t cmd_in,
                                 mp_obj_t arg_in) {
  tc32_flash_obj_t *self = MP_OBJ_TO_PTR(self_in);
  mp_int_t cmd = mp_obj_get_int(cmd_in);

  switch (cmd) {
  case MP_BLOCKDEV_IOCTL_INIT:
    return MP_OBJ_NEW_SMALL_INT(0);
  case MP_BLOCKDEV_IOCTL_DEINIT:
    return MP_OBJ_NEW_SMALL_INT(0);
  case MP_BLOCKDEV_IOCTL_SYNC:
    return MP_OBJ_NEW_SMALL_INT(0);
  case MP_BLOCKDEV_IOCTL_BLOCK_COUNT:
    return MP_OBJ_NEW_SMALL_INT(self->flash_size / SECTOR_SIZE);
  case MP_BLOCKDEV_IOCTL_BLOCK_SIZE:
    return MP_OBJ_NEW_SMALL_INT(SECTOR_SIZE);
  case MP_BLOCKDEV_IOCTL_BLOCK_ERASE:
    flash_erase_sector(mp_obj_get_int(arg_in) * SECTOR_SIZE +
                       tc32_flash_obj.flash_base);
    return MP_OBJ_NEW_SMALL_INT(0);
  default:
    return mp_const_none;
  }
}
static MP_DEFINE_CONST_FUN_OBJ_3(tc32_flash_ioctl_obj, tc32_flash_ioctl);

static const mp_rom_map_elem_t tc32_flash_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_readblocks), MP_ROM_PTR(&tc32_flash_readblocks_obj)},
    {MP_ROM_QSTR(MP_QSTR_writeblocks), MP_ROM_PTR(&tc32_flash_writeblocks_obj)},
    {MP_ROM_QSTR(MP_QSTR_ioctl), MP_ROM_PTR(&tc32_flash_ioctl_obj)},
};
static MP_DEFINE_CONST_DICT(tc32_flash_locals_dict,
                            tc32_flash_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(tc32_flash_type, MP_QSTR_Flash, MP_TYPE_FLAG_NONE,
                         make_new, tc32_flash_make_new, locals_dict,
                         &tc32_flash_locals_dict);
