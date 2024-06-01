// pins_prefix.c becomes the initial portion of the generated pins file.

#include "port.h"

#include "extmod/modmachine.h"
#include "machine_pin.h"
#include "py/mphal.h"
#include "py/obj.h"

#if defined(MCU_TLSR8232)

#define PIN(p_name, p_af0, p_af1, p_af2, p_af3, p_af4)                         \
  {                                                                            \
    {&machine_pin_type}, GPIO_##p_name, MP_QSTR_##p_name, AS_##p_af0,          \
        AS_##p_af1, AS_##p_af2, AS_##p_af3, AS_##p_af4                         \
  }

#else

#error Board not defined

#endif
