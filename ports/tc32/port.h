#pragma once

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

extern void uart_init(void);
