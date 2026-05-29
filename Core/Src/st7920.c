/*
 * st7920.c
 *
 * ST7920 128x64 LCD Driver for STM32F407VET6
 * Hardware SPI1 — PA5=SCK, PA7=MOSI, PA4=CS(GPIO), PC6=RST(GPIO)
 *
 * SPI Config (CubeMX):
 *   Mode            : Transmit Only Master
 *   Data Size       : 8 bit
 *   First Bit       : MSB
 *   Prescaler       : 64 (84MHz/64 = ~1.3MHz, ST7920 max 1MHz aman)
 *   CPOL            : High
 *   CPHA            : 2 Edge
 *   NSS             : Software Disabled
 */

#include "st7920.h"

/* ── Frame Buffer ────────────────────────────────────────────── */
uint8_t st7920_buf[ST7920_BUF_SIZE];

/* ═══════════════════════════════════════════════════════════════
 * LOW LEVEL: SPI + GPIO
 * ═══════════════════════════════════════════════════════════════ */

static inline void _cs_high(void)
{
    ST7920_CS_PORT->BSRR = ST7920_CS_PIN;
}

static inline void _cs_low(void)
{
    ST7920_CS_PORT->BSRR = (uint32_t)ST7920_CS_PIN << 16;
}

static inline void _rst_high(void)
{
    ST7920_RST_PORT->BSRR = ST7920_RST_PIN;
}

static inline void _rst_low(void)
{
    ST7920_RST_PORT->BSRR = (uint32_t)ST7920_RST_PIN << 16;
}

/*
 * Kirim 1 byte via Hardware SPI1
 */
static void _spi_write_byte(uint8_t byte)
{
    HAL_SPI_Transmit(&hspi1, &byte, 1, HAL_MAX_DELAY);
}

/*
 * ST7920 serial protocol:
 *   CMD  : sync 0xF8 + high nibble + low nibble
 *   DATA : sync 0xFA + high nibble + low nibble
 */
static void _send_cmd(uint8_t cmd)
{
    _spi_write_byte(0xF8);
    _spi_write_byte(cmd & 0xF0);
    _spi_write_byte((cmd << 4) & 0xF0);
}

static void _send_data(uint8_t data)
{
    _spi_write_byte(0xFA);
    _spi_write_byte(data & 0xF0);
    _spi_write_byte((data << 4) & 0xF0);
}

/* ═══════════════════════════════════════════════════════════════
 * INIT
 * ═══════════════════════════════════════════════════════════════ */

void ST7920_Init(void)
{
    /* Reset pulse */
    _rst_low();
    HAL_Delay(1);
    _rst_high();
    HAL_Delay(50);

    /* CS enable */
    _cs_high();
    HAL_Delay(10);

    /* Init sequence */
    _send_cmd(0x38); /* 8-bit, basic instruction */
    _send_cmd(0x08); /* display off */
    _send_cmd(0x06); /* entry mode: cursor right */
    _send_cmd(0x02); /* return home */
    _send_cmd(0x01); /* clear RAM */
    HAL_Delay(4);

    _cs_low();

    ST7920_ClearBuffer();
}

/* ═══════════════════════════════════════════════════════════════
 * SEND BUFFER
 * ═══════════════════════════════════════════════════════════════ */

void ST7920_SendBuffer(void)
{
    uint8_t y, x;

    _cs_high();
    HAL_Delay(1);

    _send_cmd(0x3E); /* extended instruction */
    _send_cmd(0x3E); /* kirim dua kali (issue #487 u8g2) */
    _send_cmd(0x36); /* enable graphic display */

    for (y = 0; y < 64; y++)
    {
        uint8_t gdram_y, gdram_x;

        if (y < 32) { gdram_y = y;      gdram_x = 0; }
        else        { gdram_y = y - 32; gdram_x = 8; }

        _send_cmd(0x80 | gdram_y);
        _send_cmd(0x80 | gdram_x);

        for (x = 0; x < 16; x++)
        {
            _send_data(st7920_buf[y * 16 + x]);
        }
    }

    _cs_low();
}

/* ═══════════════════════════════════════════════════════════════
 * BUFFER
 * ═══════════════════════════════════════════════════════════════ */

void ST7920_ClearBuffer(void)
{
    memset(st7920_buf, 0x00, ST7920_BUF_SIZE);
}

/* ═══════════════════════════════════════════════════════════════
 * PIXEL
 * ═══════════════════════════════════════════════════════════════ */

void ST7920_DrawPixel(int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || x >= ST7920_WIDTH || y < 0 || y >= ST7920_HEIGHT)
        return;

    uint16_t byte_idx = (uint16_t)(y * 16 + x / 8);
    uint8_t  bit_idx  = 7 - (x % 8);

    if (color)
        st7920_buf[byte_idx] |=  (1 << bit_idx);
    else
        st7920_buf[byte_idx] &= ~(1 << bit_idx);
}

/* ═══════════════════════════════════════════════════════════════
 * GEOMETRY
 * ═══════════════════════════════════════════════════════════════ */

void ST7920_DrawHLine(int16_t x, int16_t y, int16_t w, uint8_t color)
{
    for (int16_t i = 0; i < w; i++)
        ST7920_DrawPixel(x + i, y, color);
}

void ST7920_DrawVLine(int16_t x, int16_t y, int16_t h, uint8_t color)
{
    for (int16_t i = 0; i < h; i++)
        ST7920_DrawPixel(x, y + i, color);
}

void ST7920_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
    int16_t dx  =  (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy  = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int16_t sx  = (x0 < x1) ? 1 : -1;
    int16_t sy  = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;
    int16_t e2;

    while (1)
    {
        ST7920_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void ST7920_DrawBox(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    for (int16_t j = 0; j < h; j++)
        ST7920_DrawHLine(x, y + j, w, color);
}

void ST7920_DrawFrame(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    ST7920_DrawHLine(x,         y,         w, color);
    ST7920_DrawHLine(x,         y + h - 1, w, color);
    ST7920_DrawVLine(x,         y,         h, color);
    ST7920_DrawVLine(x + w - 1, y,         h, color);
}

static void _draw_round_corners(int16_t x, int16_t y, int16_t w, int16_t h,
                                 int16_t r, uint8_t fill, uint8_t color)
{
    int16_t cx0 = x + r,         cy0 = y + r;
    int16_t cx1 = x + w - 1 - r, cy1 = y + h - 1 - r;
    int16_t xi = 0, yi = r, d = 3 - 2 * r;

    while (yi >= xi)
    {
        if (fill)
        {
            ST7920_DrawHLine(cx0 - yi, cy0 - xi, cx1 - cx0 + 2 * yi + 1, color);
            ST7920_DrawHLine(cx0 - xi, cy0 - yi, cx1 - cx0 + 2 * xi + 1, color);
            ST7920_DrawHLine(cx0 - xi, cy1 + yi, cx1 - cx0 + 2 * xi + 1, color);
            ST7920_DrawHLine(cx0 - yi, cy1 + xi, cx1 - cx0 + 2 * yi + 1, color);
        }
        else
        {
            ST7920_DrawPixel(cx0 - xi, cy0 - yi, color);
            ST7920_DrawPixel(cx1 + xi, cy0 - yi, color);
            ST7920_DrawPixel(cx0 - yi, cy0 - xi, color);
            ST7920_DrawPixel(cx1 + yi, cy0 - xi, color);
            ST7920_DrawPixel(cx0 - xi, cy1 + yi, color);
            ST7920_DrawPixel(cx1 + xi, cy1 + yi, color);
            ST7920_DrawPixel(cx0 - yi, cy1 + xi, color);
            ST7920_DrawPixel(cx1 + yi, cy1 + xi, color);
        }
        xi++;
        if (d > 0) { yi--; d += 4 * (xi - yi) + 10; }
        else       {       d += 4 * xi + 6; }
    }
}

void ST7920_DrawRFrame(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t r, uint8_t color)
{
    if (r == 0) { ST7920_DrawFrame(x, y, w, h, color); return; }
    ST7920_DrawHLine(x + r, y,         w - 2 * r, color);
    ST7920_DrawHLine(x + r, y + h - 1, w - 2 * r, color);
    ST7920_DrawVLine(x,         y + r, h - 2 * r, color);
    ST7920_DrawVLine(x + w - 1, y + r, h - 2 * r, color);
    _draw_round_corners(x, y, w, h, r, 0, color);
}

void ST7920_DrawRBox(int16_t x, int16_t y, int16_t w, int16_t h,
                     int16_t r, uint8_t color)
{
    if (r == 0) { ST7920_DrawBox(x, y, w, h, color); return; }
    ST7920_DrawBox(x, y + r, w, h - 2 * r, color);
    _draw_round_corners(x, y, w, h, r, 1, color);
}

void ST7920_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color)
{
    int16_t x = 0, y = r, d = 3 - 2 * r;

    auto void plot(void);
    void plot(void) {
        ST7920_DrawPixel(x0 + x, y0 + y, color);
        ST7920_DrawPixel(x0 - x, y0 + y, color);
        ST7920_DrawPixel(x0 + x, y0 - y, color);
        ST7920_DrawPixel(x0 - x, y0 - y, color);
        ST7920_DrawPixel(x0 + y, y0 + x, color);
        ST7920_DrawPixel(x0 - y, y0 + x, color);
        ST7920_DrawPixel(x0 + y, y0 - x, color);
        ST7920_DrawPixel(x0 - y, y0 - x, color);
    }

    plot();
    while (y >= x)
    {
        x++;
        if (d > 0) { y--; d += 4 * (x - y) + 10; }
        else       {      d += 4 * x + 6; }
        plot();
    }
}

void ST7920_DrawDisc(int16_t x0, int16_t y0, int16_t r, uint8_t color)
{
    int16_t x = 0, y = r, d = 3 - 2 * r;

    auto void plot(void);
    void plot(void) {
        ST7920_DrawHLine(x0 - x, y0 + y, 2 * x + 1, color);
        ST7920_DrawHLine(x0 - x, y0 - y, 2 * x + 1, color);
        ST7920_DrawHLine(x0 - y, y0 + x, 2 * y + 1, color);
        ST7920_DrawHLine(x0 - y, y0 - x, 2 * y + 1, color);
    }

    plot();
    while (y >= x)
    {
        x++;
        if (d > 0) { y--; d += 4 * (x - y) + 10; }
        else       {      d += 4 * x + 6; }
        plot();
    }
}

void ST7920_DrawTriangle(int16_t x0, int16_t y0,
                         int16_t x1, int16_t y1,
                         int16_t x2, int16_t y2,
                         uint8_t color)
{
    ST7920_DrawLine(x0, y0, x1, y1, color);
    ST7920_DrawLine(x1, y1, x2, y2, color);
    ST7920_DrawLine(x2, y2, x0, y0, color);
}

/* ═══════════════════════════════════════════════════════════════
 * TEXT — Built-in 5x7 font, ASCII 32-127
 * ═══════════════════════════════════════════════════════════════ */

static const uint8_t _font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 32 space */
    {0x00,0x00,0x5F,0x00,0x00}, /* 33 ! */
    {0x00,0x07,0x00,0x07,0x00}, /* 34 " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 35 # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 36 $ */
    {0x23,0x13,0x08,0x64,0x62}, /* 37 % */
    {0x36,0x49,0x55,0x22,0x50}, /* 38 & */
    {0x00,0x05,0x03,0x00,0x00}, /* 39 ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 40 ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* 41 ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* 42 * */
    {0x08,0x08,0x3E,0x08,0x08}, /* 43 + */
    {0x00,0x50,0x30,0x00,0x00}, /* 44 , */
    {0x08,0x08,0x08,0x08,0x08}, /* 45 - */
    {0x00,0x60,0x60,0x00,0x00}, /* 46 . */
    {0x20,0x10,0x08,0x04,0x02}, /* 47 / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 48 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 49 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 50 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 51 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 52 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 53 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 54 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 55 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 56 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 57 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* 58 : */
    {0x00,0x56,0x36,0x00,0x00}, /* 59 ; */
    {0x08,0x14,0x22,0x41,0x00}, /* 60 < */
    {0x14,0x14,0x14,0x14,0x14}, /* 61 = */
    {0x00,0x41,0x22,0x14,0x08}, /* 62 > */
    {0x02,0x01,0x51,0x09,0x06}, /* 63 ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* 64 @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 65 A */
    {0x7F,0x49,0x49,0x49,0x36}, /* 66 B */
    {0x3E,0x41,0x41,0x41,0x22}, /* 67 C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 68 D */
    {0x7F,0x49,0x49,0x49,0x41}, /* 69 E */
    {0x7F,0x09,0x09,0x09,0x01}, /* 70 F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 71 G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 72 H */
    {0x00,0x41,0x7F,0x41,0x00}, /* 73 I */
    {0x20,0x40,0x41,0x3F,0x01}, /* 74 J */
    {0x7F,0x08,0x14,0x22,0x41}, /* 75 K */
    {0x7F,0x40,0x40,0x40,0x40}, /* 76 L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 77 M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 78 N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 79 O */
    {0x7F,0x09,0x09,0x09,0x06}, /* 80 P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 81 Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* 82 R */
    {0x46,0x49,0x49,0x49,0x31}, /* 83 S */
    {0x01,0x01,0x7F,0x01,0x01}, /* 84 T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 85 U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 86 V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 87 W */
    {0x63,0x14,0x08,0x14,0x63}, /* 88 X */
    {0x07,0x08,0x70,0x08,0x07}, /* 89 Y */
    {0x61,0x51,0x49,0x45,0x43}, /* 90 Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* 91 [ */
    {0x02,0x04,0x08,0x10,0x20}, /* 92 \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* 93 ] */
    {0x04,0x02,0x01,0x02,0x04}, /* 94 ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* 95 _ */
    {0x00,0x01,0x02,0x04,0x00}, /* 96 ` */
    {0x20,0x54,0x54,0x54,0x78}, /* 97 a */
    {0x7F,0x48,0x44,0x44,0x38}, /* 98 b */
    {0x38,0x44,0x44,0x44,0x20}, /* 99 c */
    {0x38,0x44,0x44,0x48,0x7F}, /* 100 d */
    {0x38,0x54,0x54,0x54,0x18}, /* 101 e */
    {0x08,0x7E,0x09,0x01,0x02}, /* 102 f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 103 g */
    {0x7F,0x08,0x04,0x04,0x78}, /* 104 h */
    {0x00,0x44,0x7D,0x40,0x00}, /* 105 i */
    {0x20,0x40,0x44,0x3D,0x00}, /* 106 j */
    {0x7F,0x10,0x28,0x44,0x00}, /* 107 k */
    {0x00,0x41,0x7F,0x40,0x00}, /* 108 l */
    {0x7C,0x04,0x18,0x04,0x78}, /* 109 m */
    {0x7C,0x08,0x04,0x04,0x78}, /* 110 n */
    {0x38,0x44,0x44,0x44,0x38}, /* 111 o */
    {0x7C,0x14,0x14,0x14,0x08}, /* 112 p */
    {0x08,0x14,0x14,0x18,0x7C}, /* 113 q */
    {0x7C,0x08,0x04,0x04,0x08}, /* 114 r */
    {0x48,0x54,0x54,0x54,0x20}, /* 115 s */
    {0x04,0x3F,0x44,0x40,0x20}, /* 116 t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 117 u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 118 v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 119 w */
    {0x44,0x28,0x10,0x28,0x44}, /* 120 x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 121 y */
    {0x44,0x64,0x54,0x4C,0x44}, /* 122 z */
    {0x00,0x08,0x36,0x41,0x00}, /* 123 { */
    {0x00,0x00,0x7F,0x00,0x00}, /* 124 | */
    {0x00,0x41,0x36,0x08,0x00}, /* 125 } */
    {0x10,0x08,0x08,0x10,0x08}, /* 126 ~ */
    {0x00,0x00,0x00,0x00,0x00}, /* 127 DEL */
};

void ST7920_DrawChar(int16_t x, int16_t y, char c)
{
    if (c < 32 || c > 127) return;
    const uint8_t *ch = _font5x7[(uint8_t)c - 32];
    for (uint8_t col = 0; col < 5; col++)
    {
        uint8_t line = ch[col];
        for (uint8_t row = 0; row < 7; row++)
        {
            if (line & (1 << row))
                ST7920_DrawPixel(x + col, y + row, 1);
        }
    }
}

void ST7920_DrawStr(int16_t x, int16_t y, const char *str)
{
    while (*str)
    {
        ST7920_DrawChar(x, y, *str++);
        x += 6;
    }
}

/* ═══════════════════════════════════════════════════════════════
 * BITMAP (XBM format, LSB first)
 * ═══════════════════════════════════════════════════════════════ */

void ST7920_DrawXBMP(int16_t x, int16_t y, int16_t w, int16_t h,
                     const uint8_t *bitmap)
{
    int16_t bw = (w + 7) / 8;
    for (int16_t j = 0; j < h; j++)
        for (int16_t i = 0; i < w; i++)
            if (bitmap[j * bw + i / 8] & (1 << (i % 8)))
                ST7920_DrawPixel(x + i, y + j, 1);
}
