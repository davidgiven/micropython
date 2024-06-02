#include "modtc32.h"
#include "port.h"
#include "py/mphal.h"
#include "py/runtime.h"

// 16 Mhz.
#define SYSTEM_CLOCK 16000000

// SPI clock = SYSTEM_CLOCK / (2 * (SPI_DIV + 1))
// For exaxmple, by setting it to 15, we get 2 uS SPI clock period.
#define SPI_DIV 3

// SDA/MOSI.
#define PIN_SDA GPIO_PC3
// SCL/Clock.
#define PIN_SCL GPIO_PC5
// DC/RS Data selection (labeled RS in the display datasheet).
#define PIN_DC GPIO_PC6
// Chip select, active low.
#define PIN_CS GPIO_PA1
// RST pin, active low.
#define PIN_RST GPIO_PA0
// LED panel catode transistor base pin, active low.
#define PIN_LEDKBASE GPIO_PA2
// LED connected to the TX labeled pin.
#define PIN_LED GPIO_PB4

// Normal mode (not partial).
#define TFT_CMD_NRON 0x13

#define TFT_CMD_ON 0x29
// Memory access pattern.
#define TFT_CMD_MADCTL 0x36
// Column set.
#define TFT_CMD_CASET 0x2a
// Row set.
#define TFT_CMD_RASET 0x2b
// Pixel format.
#define TFT_CMD_COLMOD 0x3a
// Signals the start of image drawing.
#define TFT_CMD_RAMWR 0x2c
// Sleep out.
#define TFT_CMD_SLEEPOUT 0x11
// Software reset.
#define TFT_CMD_SWRESET 0x01
// Set inversion on.
#define TFT_CMD_INVON 0x21
// Configure partial display area (page 206).
#define TFT_CMD_PTLAR 0x30
// Configure vertical scrolling area (page 208).
#define TFT_CMD_VSCRDEF 0x33
// Set vertical scroll position (page 218).
#define TFT_CMD_VSCSAD 0x37

// When the TFT is screen side up, ribbon connector on top:
// 1. Without any mirroring:
// x <----.
//        |
//        |
//        \/ y
// x-direction
//
// 1. With X-mirroring:
// .-----> x
// |
// |
// \/ y
// 1. With Y-mirroring:
//        /\ y
//        |
//        |
// x <----.
// x-direction

// #define TFT_X_OFFSET 26
// #define TFT_Y_OFFSET 1

// 16-bit RGB-565 format.
#define TFT_COLOR_BLACK 0x0000
#define TFT_COLOR_WHITE 0xffff
#define TFT_COLOR_RED 0xf800
#define TFT_COLOR_GREEN 0x07e0
#define TFT_COLOR_BLUE 0x001f

typedef struct _screen_obj_t {
  mp_obj_base_t base;
  uint16_t width;
  uint16_t height;
  uint16_t xoffset;
  uint16_t yoffset;
} screen_obj_t;

#define SECTOR_SIZE 4096

/* Our singleton flash storage object. */
static screen_obj_t screen_obj = {{&tc32_screen_type}};

typedef struct {
  uint8_t cmd;
  uint8_t sleep_ms;
  uint8_t args_len;
  uint8_t args[];
} tft_cmd_t;

static void spi_wait(void) {
  while (reg_spi_ctrl & FLD_SPI_BUSY)
    ;
}

static void spi_write8(uint8_t byte) {
  reg_spi_ctrl &= ~(FLD_SPI_DATA_OUT_DIS | FLD_SPI_RD);
  spi_wait();
  reg_spi_data = byte;
  spi_wait();
}

static uint8_t spi_read8(void) {
  reg_spi_ctrl |= FLD_SPI_DATA_OUT_DIS | FLD_SPI_RD;
  spi_wait();
  return reg_spi_data;
}

static void spi_write16(uint16_t data) {
  spi_write8(data >> 8);
  spi_write8(data & 0xff);
}

static void spi_send_cmd(uint8_t cmd) {
  // Command mode - DC low.
  gpio_write(PIN_DC, 0);
  spi_write8(cmd);
  // Data mode - DC high.
  gpio_write(PIN_DC, 1);
}

static void tft_set_window(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  // Set column.
  spi_send_cmd(TFT_CMD_CASET);
  x += screen_obj.xoffset;
  spi_write16(x);
  spi_write16(x + w - 1);
  // Set row.
  spi_send_cmd(TFT_CMD_RASET);
  y += screen_obj.yoffset;
  spi_write16(y);
  spi_write16(y + h - 1);
  // Signal that we will send image data next.
  spi_send_cmd(TFT_CMD_RAMWR);
}

static void tft_draw_pixel(uint8_t x, uint8_t y, uint16_t color) {
  gpio_write(PIN_CS, 0);
  tft_set_window(x, y, 1, 1);
  spi_write16(color);
  gpio_write(PIN_CS, 1);
}

static void tft_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                          uint16_t color) {
  gpio_write(PIN_CS, 0);
  tft_set_window(x, y, w, h);
  for (int i = 0; i < w * h; i++) {
    spi_write16(color);
  }
  gpio_write(PIN_CS, 1);
}

static void tft_line(mp_int_t x1, mp_int_t y1, mp_int_t x2, mp_int_t y2,
                     uint16_t col) {
  mp_int_t dx = x2 - x1;
  mp_int_t sx;
  if (dx > 0) {
    sx = 1;
  } else {
    dx = -dx;
    sx = -1;
  }

  mp_int_t dy = y2 - y1;
  mp_int_t sy;
  if (dy > 0) {
    sy = 1;
  } else {
    dy = -dy;
    sy = -1;
  }

  bool steep;
  if (dy > dx) {
    mp_int_t temp;
    temp = x1;
    x1 = y1;
    y1 = temp;
    temp = dx;
    dx = dy;
    dy = temp;
    temp = sx;
    sx = sy;
    sy = temp;
    steep = true;
  } else {
    steep = false;
  }

  mp_int_t e = 2 * dy - dx;
  for (mp_int_t i = 0; i < dx; ++i) {
    if (steep) {
      if (0 <= y1 && y1 < screen_obj.width && 0 <= x1 &&
          x1 < screen_obj.height) {
        tft_draw_pixel(y1, x1, col);
      }
    } else {
      if (0 <= x1 && x1 < screen_obj.width && 0 <= y1 &&
          y1 < screen_obj.height) {
        tft_draw_pixel(x1, y1, col);
      }
    }
    while (e >= 0) {
      y1 += sy;
      e -= 2 * dx;
    }
    x1 += sx;
    e += 2 * dy;
  }

  if (0 <= x2 && x2 < screen_obj.width && 0 <= y2 && y2 < screen_obj.height) {
    tft_draw_pixel(x2, y2, col);
  }
}

static void tft_clear_screen() {
  return tft_draw_rect(0, 0, screen_obj.width, screen_obj.height,
                       TFT_COLOR_BLACK);
}

void screen_init() {
  // MODE0 means SDA (MOSI) is sampled at the leading edge of SCK.
  spi_master_init(SPI_DIV, SPI_MODE0);
  reg_spi_ctrl |= FLD_SPI_SHARE_MODE;

  // SDA, SCL setup.
  gpio_set_func(PIN_SDA, AS_SPI_MDO);
  gpio_set_func(PIN_SCL, AS_SPI_MCK);
  gpio_set_input_en(PIN_SDA, 1);
  gpio_set_output_en(PIN_SDA, 1);
  gpio_set_output_en(PIN_SCL, 1);

  gpio_set_data_strength(PIN_SDA, 0);
  gpio_set_data_strength(PIN_SCL, 0);

  // CS setup.
  gpio_set_func(PIN_CS, AS_GPIO);
  gpio_set_input_en(PIN_CS, 0);
  gpio_set_output_en(PIN_CS, 1);
  gpio_write(PIN_CS, 1);

  // DC setup.
  gpio_set_func(PIN_DC, AS_GPIO);
  gpio_set_input_en(PIN_DC, 0);
  gpio_set_output_en(PIN_DC, 1);
  gpio_write(PIN_DC, 0);

  // LEDKBASE setup.
  gpio_set_func(PIN_LEDKBASE, AS_GPIO);
  gpio_set_input_en(PIN_LEDKBASE, 0);
  gpio_set_output_en(PIN_LEDKBASE, 1);
  gpio_write(PIN_LEDKBASE, 1);

  // RST setup - active low.
  gpio_set_func(PIN_RST, AS_GPIO);
  gpio_set_input_en(PIN_RST, 0);
  gpio_set_output_en(PIN_RST, 1);
  gpio_write(PIN_RST, 1);

  // Reset pin.
  gpio_write(PIN_RST, 0);
  sleep_ms(10);
  gpio_write(PIN_RST, 1);
  sleep_ms(10);

  /* Initialise the module. */

  gpio_write(PIN_CS, 0);

  spi_send_cmd(TFT_CMD_SWRESET);
  sleep_ms(120);
  spi_send_cmd(TFT_CMD_SLEEPOUT);
  sleep_ms(120);

  // Enable internal commands.
  spi_send_cmd(0xfe);
  spi_send_cmd(0xfe);
  spi_send_cmd(0xfe);
  spi_send_cmd(0xef);

  // Weird magical stuff to configure the GC9106 to override the geometry. For
  // some reason it's wired with GM=01, which means it's in 132x132 mode. These
  // commands --- somehow --- override this to GM=11, or 128x160 mode.
  spi_send_cmd(0xb6);
  spi_write8(0x11);
  sleep_us(10);

  spi_send_cmd(0xac);
  spi_write8(0x0b);
  sleep_us(10);

  // Memory access direction - it controls the direction the pixels are written
  // to the display.
  // 0x00 - no mirroring
  // 0x40 - x mirroring
  // 0x80 - y mirroring
  // 0xc0 - x and y mirroring
  // Also:
  // 0x08 - swap BGR/RGB order
  //
  spi_send_cmd(TFT_CMD_MADCTL);
  spi_write8(0x88);
  sleep_us(10);

  // Set pixel color format (565 RGB - 16 bits per pixel).
  spi_send_cmd(TFT_CMD_COLMOD);
  spi_write8(0x55);
  sleep_us(10);

  // In this module, it seems like _enabling_ inversion produces the expected
  // colors. If we don't enable this, colors look inverted by default.
  spi_send_cmd(TFT_CMD_INVON);
  sleep_us(10);

  // Normal mode --- display the entire framebuffer on the screen.
  spi_send_cmd(TFT_CMD_NRON);
  sleep_us(10);

  // Turn on the display.
  spi_send_cmd(TFT_CMD_ON);
  sleep_us(120);

  gpio_write(PIN_CS, 1);

  tft_clear_screen();
}

static mp_obj_t screen_obj_init_helper(size_t n_args, const mp_obj_t *pos_args,
                                       mp_map_t *kw_args) {
  /* Parse arguments. */

  enum { ARG_width, ARG_height, ARG_xoffset, ARG_yoffset };
  static const mp_arg_t allowed_args[] = {
      {MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
      {MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
      {MP_QSTR_xoffset, MP_ARG_INT, {.u_int = 0}},
      {MP_QSTR_yoffset, MP_ARG_INT, {.u_int = 0}},
  };

  mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
  mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args),
                   allowed_args, args);

  screen_obj.width = args[ARG_width].u_int;
  screen_obj.height = args[ARG_height].u_int;
  screen_obj.xoffset = args[ARG_xoffset].u_int;
  screen_obj.yoffset = args[ARG_yoffset].u_int;

  mp_printf(&mp_plat_print, "width=%d height=%d xoffset=%d yoffset=%d\n",
            screen_obj.width, screen_obj.height, screen_obj.xoffset,
            screen_obj.yoffset);
  screen_init();

  // Return singleton object.
  return &screen_obj;
}

static void screen_args(const mp_obj_t *args_in, mp_int_t *args_out, int n) {
  for (int i = 0; i < n; ++i) {
    args_out[i] = mp_obj_get_int(args_in[i + 1]);
  }
}

mp_obj_t screen_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw,
                         const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, /* n_args_min= */ 0,
                   /* n_args_max= */ MP_OBJ_FUN_ARGS_MAX, /* takes_kw= */ true);

  mp_map_t kw_args;
  mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);

  return screen_obj_init_helper(n_args, args, &kw_args);
}

/* Warning: this doesn't work. Don't know why. */
static mp_obj_t screen_readreg(size_t n_args, const mp_obj_t *args_in) {
  (void)n_args;

  mp_int_t args[1]; // x1, y1, x2, y2, col
  screen_args(args_in, args, 1);

  gpio_write(PIN_CS, 0);
  spi_send_cmd(args[0]);
  spi_read8(); // dummy, ignored
  uint8_t value = spi_read8();
  gpio_write(PIN_CS, 1);

  return MP_OBJ_NEW_SMALL_INT(value);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_readreg_obj, 2, 2,
                                           screen_readreg);

static mp_obj_t screen_line(size_t n_args, const mp_obj_t *args_in) {
  (void)n_args;

  mp_int_t args[5]; // x1, y1, x2, y2, col
  screen_args(args_in, args, 5);

  tft_line(args[0], args[1], args[2], args[3], args[4]);
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_line_obj, 6, 6, screen_line);

static mp_obj_t screen_fill_rect(size_t n_args, const mp_obj_t *args_in) {
  mp_int_t args[5]; // x, y, w, h, col
  screen_args(args_in, args, 5);
  tft_draw_rect(args[0], args[1], args[2], args[3], args[4]);
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_fill_rect_obj, 6, 6,
                                           screen_fill_rect);

static mp_obj_t screen_pixel(size_t n_args, const mp_obj_t *args_in) {
  mp_int_t x = mp_obj_get_int(args_in[1]);
  mp_int_t y = mp_obj_get_int(args_in[2]);
  if (0 <= x && x < screen_obj.width && 0 <= y && y < screen_obj.height) {
    // set
    tft_draw_pixel(x, y, mp_obj_get_int(args_in[3]));
  }
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_pixel_obj, 4, 4,
                                           screen_pixel);

static const mp_rom_map_elem_t screen_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_readreg), MP_ROM_PTR(&screen_readreg_obj)},
#if 0
    { MP_ROM_QSTR(MP_QSTR_fill), MP_ROM_PTR(&screen_fill_obj) },
#endif
    {MP_ROM_QSTR(MP_QSTR_fill_rect), MP_ROM_PTR(&screen_fill_rect_obj)},
    {MP_ROM_QSTR(MP_QSTR_pixel), MP_ROM_PTR(&screen_pixel_obj)},
#if 0
    { MP_ROM_QSTR(MP_QSTR_hline), MP_ROM_PTR(&screen_hline_obj) },
    { MP_ROM_QSTR(MP_QSTR_vline), MP_ROM_PTR(&screen_vline_obj) },
    { MP_ROM_QSTR(MP_QSTR_rect), MP_ROM_PTR(&screen_rect_obj) },
#endif
    {MP_ROM_QSTR(MP_QSTR_line), MP_ROM_PTR(&screen_line_obj)},
#if 0
    { MP_ROM_QSTR(MP_QSTR_ellipse), MP_ROM_PTR(&screen_ellipse_obj) },
#if MICROPY_PY_ARRAY
    { MP_ROM_QSTR(MP_QSTR_poly), MP_ROM_PTR(&screen_poly_obj) },
#endif
    { MP_ROM_QSTR(MP_QSTR_blit), MP_ROM_PTR(&screen_blit_obj) },
    { MP_ROM_QSTR(MP_QSTR_scroll), MP_ROM_PTR(&screen_scroll_obj) },
    { MP_ROM_QSTR(MP_QSTR_text), MP_ROM_PTR(&screen_text_obj) },
#endif
};
static MP_DEFINE_CONST_DICT(screen_locals_dict, screen_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(tc32_screen_type, MP_QSTR_Screen, MP_TYPE_FLAG_NONE,
                         make_new, screen_make_new, locals_dict,
                         &screen_locals_dict);
