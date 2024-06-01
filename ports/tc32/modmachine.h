#ifndef MICROPY_INCLUDED_TC32_MODMACHINE_H
#define MICROPY_INCLUDED_TC32_MODMACHINE_H

#include "py/obj.h"

void machine_init(void);
void machine_deinit(void);

void machine_pins_init(void);
void machine_pins_deinit(void);

struct _machine_spi_obj_t *spi_from_mp_obj(mp_obj_t o);

#endif // MICROPY_INCLUDED_TC32_MODMACHINE_H
