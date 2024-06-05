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

#include "modtc32.h"
#include "machine_pin.h"
#include "py/mphal.h"
#include "py/runtime.h"

#define BATTERY_SAMPLE_NUM 8
static mp_obj_t tc32_battery_mv(void) {
  static bool adc_hw_initialized = 0;
  if (!adc_hw_initialized) {
    adc_hw_initialized = 1;
    adc_init();
  }

  adc_set_misc_rns_capture_state_length(0xf0); // max_mc
  adc_set_all_set_state_length(0x0a);          // max_s

  adc_set_chn_en(ADC_MISC_CHN);
  adc_set_max_state_cnt(0x02);

  adc_set_all_vref(ADC_MISC_CHN, ADC_VREF_1P2V);
  adc_set_vref_vbat_div(ADC_VBAT_DIVIDER_OFF);

  adc_set_misc_n_ain(GND);
  adc_set_misc_p_ain(VBAT);
  adc_set_misc_input_mode(DIFFERENTIAL_MODE);

  adc_set_all_resolution(ADC_MISC_CHN, RES14);
  adc_set_all_tsample_cycle(ADC_MISC_CHN, SAMPLING_CYCLES_6);
  adc_set_all_ain_pre_scaler(ADC_PRESCALER_1F4);
  adc_set_mode(ADC_NORMAL_MODE);

  return MP_OBJ_NEW_SMALL_INT(adc_set_sample_and_get_result());
}
static MP_DEFINE_CONST_FUN_OBJ_0(tc32_battery_mv_obj, tc32_battery_mv);

static const mp_rom_map_elem_t tc32_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tc32)},
    {MP_ROM_QSTR(MP_QSTR_Flash), MP_ROM_PTR(&tc32_flash_type)},
    {MP_ROM_QSTR(MP_QSTR_Screen), MP_ROM_PTR(&tc32_screen_type)},
    {MP_ROM_QSTR(MP_QSTR_battery_mv), MP_ROM_PTR(&tc32_battery_mv_obj)},
};
static MP_DEFINE_CONST_DICT(tc32_module_globals, tc32_module_globals_table);

const mp_obj_module_t mp_module_tc32 = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&tc32_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_tc32, mp_module_tc32);
