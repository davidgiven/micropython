#include "modtc32.h"
#include "port.h"
#include "py/binary.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include <stdlib.h>

#include "extmod/font_petme128_8x8.h"

// 16 Mhz.
#define SYSTEM_CLOCK 16000000

// SPI clock = SYSTEM_CLOCK / (2 * (SPI_DIV + 1))
// For exaxmple, by setting it to 15, we get 2 uS SPI clock period.
#define SPI_DIV 1

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

static void setpixel(uint8_t x, uint8_t y, uint16_t color) {
  gpio_write(PIN_CS, 0);
  tft_set_window(x, y, 1, 1);
  spi_write16(color);
  gpio_write(PIN_CS, 1);
}

static void fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                      uint16_t color) {
  gpio_write(PIN_CS, 0);
  tft_set_window(x, y, w, h);
  for (int i = 0; i < w * h; i++) {
    spi_write16(color);
  }
  gpio_write(PIN_CS, 1);
}

static void line(mp_int_t x1, mp_int_t y1, mp_int_t x2, mp_int_t y2,
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
        setpixel(y1, x1, col);
      }
    } else {
      if (0 <= x1 && x1 < screen_obj.width && 0 <= y1 &&
          y1 < screen_obj.height) {
        setpixel(x1, y1, col);
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
    setpixel(x2, y2, col);
  }
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
  spi_write8(0xc8);
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

  // Clear the screen and then turn on the display.
  spi_send_cmd(TFT_CMD_ON);
  sleep_us(120);

  gpio_write(PIN_CS, 1);
  fill_rect(0, 0, screen_obj.width, screen_obj.height, TFT_COLOR_BLACK);
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

  line(args[0], args[1], args[2], args[3], args[4]);
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_line_obj, 6, 6, screen_line);

static mp_obj_t screen_fill(mp_obj_t self_in, mp_obj_t col_in) {
  mp_int_t col = mp_obj_get_int(col_in);
  fill_rect(0, 0, screen_obj.width, screen_obj.height, col);
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(screen_fill_obj, screen_fill);

static mp_obj_t screen_fill_rect(size_t n_args, const mp_obj_t *args_in) {
  mp_int_t args[5]; // x, y, w, h, col
  screen_args(args_in, args, 5);
  fill_rect(args[0], args[1], args[2], args[3], args[4]);
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_fill_rect_obj, 6, 6,
                                           screen_fill_rect);

static mp_obj_t screen_hline(size_t n_args, const mp_obj_t *args_in) {
  (void)n_args;

  mp_int_t args[4]; // x, y, w, col
  screen_args(args_in, args, 4);

  fill_rect(args[0], args[1], args[2], 1, args[3]);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_hline_obj, 5, 5,
                                           screen_hline);

static mp_obj_t screen_vline(size_t n_args, const mp_obj_t *args_in) {
  (void)n_args;

  mp_int_t args[4]; // x, y, h, col
  screen_args(args_in, args, 4);

  fill_rect(args[0], args[1], 1, args[2], args[3]);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_vline_obj, 5, 5,
                                           screen_vline);

static mp_obj_t screen_rect(size_t n_args, const mp_obj_t *args_in) {
  mp_int_t args[5]; // x, y, w, h, col
  screen_args(args_in, args, 5);
  if (n_args > 6 && mp_obj_is_true(args_in[6])) {
    fill_rect(args[0], args[1], args[2], args[3], args[4]);
  } else {
    fill_rect(args[0], args[1], args[2], 1, args[4]);
    fill_rect(args[0], args[1] + args[3] - 1, args[2], 1, args[4]);
    fill_rect(args[0], args[1], 1, args[3], args[4]);
    fill_rect(args[0] + args[2] - 1, args[1], 1, args[3], args[4]);
  }
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_rect_obj, 6, 7, screen_rect);

static void setpixel_checked(mp_int_t x, mp_int_t y, mp_int_t col,
                             mp_int_t mask) {
  if (mask && 0 <= x && x < screen_obj.width && 0 <= y &&
      y < screen_obj.height) {
    setpixel(x, y, col);
  }
}

static mp_obj_t screen_pixel(size_t n_args, const mp_obj_t *args_in) {
  mp_int_t x = mp_obj_get_int(args_in[1]);
  mp_int_t y = mp_obj_get_int(args_in[2]);
  setpixel_checked(x, y, mp_obj_get_int(args_in[3]), 1);
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_pixel_obj, 4, 4,
                                           screen_pixel);

// Q2 Q1
// Q3 Q4
#define ELLIPSE_MASK_FILL (0x10)
#define ELLIPSE_MASK_ALL (0x0f)
#define ELLIPSE_MASK_Q1 (0x01)
#define ELLIPSE_MASK_Q2 (0x02)
#define ELLIPSE_MASK_Q3 (0x04)
#define ELLIPSE_MASK_Q4 (0x08)

static void draw_ellipse_points(mp_int_t cx, mp_int_t cy, mp_int_t x,
                                mp_int_t y, mp_int_t col, mp_int_t mask) {
  if (mask & ELLIPSE_MASK_FILL) {
    if (mask & ELLIPSE_MASK_Q1) {
      fill_rect(cx, cy - y, x + 1, 1, col);
    }
    if (mask & ELLIPSE_MASK_Q2) {
      fill_rect(cx - x, cy - y, x + 1, 1, col);
    }
    if (mask & ELLIPSE_MASK_Q3) {
      fill_rect(cx - x, cy + y, x + 1, 1, col);
    }
    if (mask & ELLIPSE_MASK_Q4) {
      fill_rect(cx, cy + y, x + 1, 1, col);
    }
  } else {
    setpixel_checked(cx + x, cy - y, col, mask & ELLIPSE_MASK_Q1);
    setpixel_checked(cx - x, cy - y, col, mask & ELLIPSE_MASK_Q2);
    setpixel_checked(cx - x, cy + y, col, mask & ELLIPSE_MASK_Q3);
    setpixel_checked(cx + x, cy + y, col, mask & ELLIPSE_MASK_Q4);
  }
}

static mp_obj_t screen_ellipse(size_t n_args, const mp_obj_t *args_in) {
  mp_int_t args[5];
  screen_args(args_in, args, 5); // cx, cy, xradius, yradius, col
  mp_int_t mask =
      (n_args > 6 && mp_obj_is_true(args_in[6])) ? ELLIPSE_MASK_FILL : 0;
  if (n_args > 7) {
    mask |= mp_obj_get_int(args_in[7]) & ELLIPSE_MASK_ALL;
  } else {
    mask |= ELLIPSE_MASK_ALL;
  }
  mp_int_t two_asquare = 2 * args[2] * args[2];
  mp_int_t two_bsquare = 2 * args[3] * args[3];
  mp_int_t x = args[2];
  mp_int_t y = 0;
  mp_int_t xchange = args[3] * args[3] * (1 - 2 * args[2]);
  mp_int_t ychange = args[2] * args[2];
  mp_int_t ellipse_error = 0;
  mp_int_t stoppingx = two_bsquare * args[2];
  mp_int_t stoppingy = 0;
  while (stoppingx >= stoppingy) { // 1st set of points,  y' > -1
    draw_ellipse_points(args[0], args[1], x, y, args[4], mask);
    y += 1;
    stoppingy += two_asquare;
    ellipse_error += ychange;
    ychange += two_asquare;
    if ((2 * ellipse_error + xchange) > 0) {
      x -= 1;
      stoppingx -= two_bsquare;
      ellipse_error += xchange;
      xchange += two_bsquare;
    }
  }
  // 1st point set is done start the 2nd set of points
  x = 0;
  y = args[3];
  xchange = args[3] * args[3];
  ychange = args[2] * args[2] * (1 - 2 * args[3]);
  ellipse_error = 0;
  stoppingx = 0;
  stoppingy = two_asquare * args[3];
  while (stoppingx <= stoppingy) { // 2nd set of points, y' < -1
    draw_ellipse_points(args[0], args[1], x, y, args[4], mask);
    x += 1;
    stoppingx += two_bsquare;
    ellipse_error += xchange;
    xchange += two_bsquare;
    if ((2 * ellipse_error + ychange) > 0) {
      y -= 1;
      stoppingy -= two_asquare;
      ellipse_error += ychange;
      ychange += two_asquare;
    }
  }
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_ellipse_obj, 6, 8,
                                           screen_ellipse);

static mp_int_t poly_int(mp_buffer_info_t *bufinfo, size_t index) {
  return mp_obj_get_int(
      mp_binary_get_val_array(bufinfo->typecode, bufinfo->buf, index));
}

static mp_obj_t screen_poly(size_t n_args, const mp_obj_t *args_in) {
  mp_int_t x = mp_obj_get_int(args_in[1]);
  mp_int_t y = mp_obj_get_int(args_in[2]);

  mp_buffer_info_t bufinfo;
  mp_get_buffer_raise(args_in[3], &bufinfo, MP_BUFFER_READ);
  // If an odd number of values was given, this rounds down to multiple of two.
  int n_poly =
      bufinfo.len / (mp_binary_get_size('@', bufinfo.typecode, NULL) * 2);

  if (n_poly == 0) {
    return mp_const_none;
  }

  mp_int_t col = mp_obj_get_int(args_in[4]);
  bool fill = n_args > 5 && mp_obj_is_true(args_in[5]);

  if (fill) {
    // This implements an integer version of
    // http://alienryderflex.com/polygon_fill/

    // The idea is for each scan line, compute the sorted list of x
    // coordinates where the scan line intersects the polygon edges,
    // then fill between each resulting pair.

    // Restrict just to the scan lines that include the vertical extent of
    // this polygon.
    mp_int_t y_min = INT_MAX, y_max = INT_MIN;
    for (int i = 0; i < n_poly; i++) {
      mp_int_t py = poly_int(&bufinfo, i * 2 + 1);
      y_min = MIN(y_min, py);
      y_max = MAX(y_max, py);
    }

    for (mp_int_t row = y_min; row <= y_max; row++) {
      // Each node is the x coordinate where an edge crosses this scan line.
      mp_int_t nodes[n_poly];
      int n_nodes = 0;
      mp_int_t px1 = poly_int(&bufinfo, 0);
      mp_int_t py1 = poly_int(&bufinfo, 1);
      int i = n_poly * 2 - 1;
      do {
        mp_int_t py2 = poly_int(&bufinfo, i--);
        mp_int_t px2 = poly_int(&bufinfo, i--);

        // Don't include the bottom pixel of a given edge to avoid
        // duplicating the node with the start of the next edge. This
        // will miss some pixels on the boundary, and in particular
        // at a local minima or inflection point.
        if (py1 != py2 &&
            ((py1 > row && py2 <= row) || (py1 <= row && py2 > row))) {
          mp_int_t node =
              (32 * px1 + 32 * (px2 - px1) * (row - py1) / (py2 - py1) + 16) /
              32;
          nodes[n_nodes++] = node;
        } else if (row == MAX(py1, py2)) {
          // At local-minima, try and manually fill in the pixels that get
          // missed above.
          if (py1 < py2) {
            setpixel_checked(x + px2, y + py2, col, 1);
          } else if (py2 < py1) {
            setpixel_checked(x + px1, y + py1, col, 1);
          } else {
            // Even though this is a hline and would be faster to
            // use fill_rect, use line() because it handles x2 <
            // x1.
            line(x + px1, y + py1, x + px2, y + py2, col);
          }
        }

        px1 = px2;
        py1 = py2;
      } while (i >= 0);

      if (!n_nodes) {
        continue;
      }

      // Sort the nodes left-to-right (bubble-sort for code size).
      i = 0;
      while (i < n_nodes - 1) {
        if (nodes[i] > nodes[i + 1]) {
          mp_int_t swap = nodes[i];
          nodes[i] = nodes[i + 1];
          nodes[i + 1] = swap;
          if (i) {
            i--;
          }
        } else {
          i++;
        }
      }

      // Fill between each pair of nodes.
      for (i = 0; i < n_nodes; i += 2) {
        fill_rect(x + nodes[i], y + row, (nodes[i + 1] - nodes[i]) + 1, 1, col);
      }
    }
  } else {
    // Outline only.
    mp_int_t px1 = poly_int(&bufinfo, 0);
    mp_int_t py1 = poly_int(&bufinfo, 1);
    int i = n_poly * 2 - 1;
    do {
      mp_int_t py2 = poly_int(&bufinfo, i--);
      mp_int_t px2 = poly_int(&bufinfo, i--);
      line(x + px1, y + py1, x + px2, y + py2, col);
      px1 = px2;
      py1 = py2;
    } while (i >= 0);
  }

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(screen_poly_obj, 5, 6, screen_poly);

static void drawchar(char c, int x, int y, int fg, int bg, int rx, int ry,
                     int dx, int dy) {
  const uint8_t *cd = &font_petme128_8x8[(c - 32) * 8];
  // loop over char data
  for (int j = 0; j < 8; j++) {
    uint8_t column = *cd++;
    for (int k = 0; k < 8; k++) {
      int c = (column & 1) ? fg : bg;
      if (c != -1) {
        int xx1 = x + rx * j + dx * k;
        int yy1 = y + ry * j + dy * k;
        int xx2 = xx1 + rx + dx;
        int yy2 = yy1 + ry + dy;
        fill_rect(MIN(xx1, xx2), MIN(yy1, yy2), abs(rx + dx), abs(ry + dy), c);
      }
      column >>= 1;
    }
  }
}

static mp_obj_t screen_text(size_t n_args, const mp_obj_t *pos_args,
                            mp_map_t *kw_args) {
  static const int8_t directions[4][2][2] = {{{1, 0}, {0, 1}},
                                             {{0, 1}, {-1, 0}},
                                             {{-1, 0}, {0, -1}},
                                             {{0, -1}, {1, 0}}};

  enum {
    ARG_self,
    ARG_text,
    ARG_x,
    ARG_y,
    ARG_fg,
    ARG_bg,
    ARG_dir,
    ARG_xscale,
    ARG_yscale
  };

  static const mp_arg_t allowed_args[] = {
      {MP_QSTR_self, MP_ARG_REQUIRED | MP_ARG_OBJ},
      {MP_QSTR_text, MP_ARG_REQUIRED | MP_ARG_OBJ},
      {MP_QSTR_x, MP_ARG_REQUIRED | MP_ARG_INT},
      {MP_QSTR_y, MP_ARG_REQUIRED | MP_ARG_INT},
      {MP_QSTR_fg, MP_ARG_INT, {.u_int = 0xffff}},
      {MP_QSTR_bg, MP_ARG_INT, {.u_int = -1}},
      {MP_QSTR_dir, MP_ARG_INT, {.u_int = 0}},
      {MP_QSTR_xscale, MP_ARG_INT, {.u_int = 1}},
      {MP_QSTR_yscale, MP_ARG_INT, {.u_int = 1}}};

  mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
  mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args),
                   allowed_args, args);

  const char *str = mp_obj_str_get_str(args[ARG_text].u_obj);
  mp_int_t x = args[ARG_x].u_int;
  mp_int_t y = args[ARG_y].u_int;
  mp_int_t fg = args[ARG_fg].u_int;
  mp_int_t bg = args[ARG_bg].u_int;
  mp_int_t dir = args[ARG_dir].u_int;
  mp_int_t xscale = args[ARG_xscale].u_int;
  mp_int_t yscale = args[ARG_yscale].u_int;
  mp_int_t rx = directions[dir][0][0] * xscale;
  mp_int_t ry = directions[dir][0][1] * yscale;
  mp_int_t dx = directions[dir][1][0] * xscale;
  mp_int_t dy = directions[dir][1][1] * yscale;

  // loop over chars
  for (; *str; ++str) {
    // get char and make sure its in range of font
    int chr = *(uint8_t *)str;
    if (chr < 32 || chr > 127) {
      chr = 127;
    }
    drawchar(chr, x, y, fg, bg, rx, ry, dx, dy);
    x += rx * 8;
    y += ry * 8;
  }
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(screen_text_obj, 4, screen_text);

static const mp_rom_map_elem_t screen_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_readreg), MP_ROM_PTR(&screen_readreg_obj)},
    {MP_ROM_QSTR(MP_QSTR_fill), MP_ROM_PTR(&screen_fill_obj)},
    {MP_ROM_QSTR(MP_QSTR_fill_rect), MP_ROM_PTR(&screen_fill_rect_obj)},
    {MP_ROM_QSTR(MP_QSTR_pixel), MP_ROM_PTR(&screen_pixel_obj)},
    {MP_ROM_QSTR(MP_QSTR_hline), MP_ROM_PTR(&screen_hline_obj)},
    {MP_ROM_QSTR(MP_QSTR_vline), MP_ROM_PTR(&screen_vline_obj)},
    {MP_ROM_QSTR(MP_QSTR_rect), MP_ROM_PTR(&screen_rect_obj)},
    {MP_ROM_QSTR(MP_QSTR_line), MP_ROM_PTR(&screen_line_obj)},
    {MP_ROM_QSTR(MP_QSTR_ellipse), MP_ROM_PTR(&screen_ellipse_obj)},
    {MP_ROM_QSTR(MP_QSTR_poly), MP_ROM_PTR(&screen_poly_obj)},
    {MP_ROM_QSTR(MP_QSTR_text), MP_ROM_PTR(&screen_text_obj)},
};
static MP_DEFINE_CONST_DICT(screen_locals_dict, screen_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(tc32_screen_type, MP_QSTR_Screen, MP_TYPE_FLAG_NONE,
                         make_new, screen_make_new, locals_dict,
                         &screen_locals_dict);
