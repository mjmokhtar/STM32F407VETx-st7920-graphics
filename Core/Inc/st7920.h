/*
 * st7920.h
 *
 * ST7920 128x64 LCD Driver for STM32F407VET6
 * Hardware SPI1 (PA5=SCK, PA7=MOSI, PA4=CS, PC6=RST)
 * Translated from U8g2 library (olikraus)
 */

#ifndef ST7920_H
#define ST7920_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <string.h>

/* ── SPI Handle ──────────────────────────────────────────────── */
/* Pastikan sesuai dengan yang di-generate CubeMX di main.c      */
extern SPI_HandleTypeDef hspi1;

/* ── Pin Definitions ─────────────────────────────────────────── */
#define ST7920_CS_PORT      GPIOA
#define ST7920_CS_PIN       GPIO_PIN_4

#define ST7920_RST_PORT     GPIOC
#define ST7920_RST_PIN      GPIO_PIN_6

/* ── Display Dimensions ──────────────────────────────────────── */
#define ST7920_WIDTH        128
#define ST7920_HEIGHT       64
#define ST7920_BUF_SIZE     (ST7920_WIDTH * ST7920_HEIGHT / 8)  /* 1024 bytes */

/* ── Frame Buffer ────────────────────────────────────────────── */
extern uint8_t st7920_buf[ST7920_BUF_SIZE];

/* ── Public API ──────────────────────────────────────────────── */

/* Init & control */
void ST7920_Init(void);
void ST7920_SendBuffer(void);
void ST7920_ClearBuffer(void);

/* Pixel */
void ST7920_DrawPixel(int16_t x, int16_t y, uint8_t color);

/* Geometry */
void ST7920_DrawHLine(int16_t x, int16_t y, int16_t w, uint8_t color);
void ST7920_DrawVLine(int16_t x, int16_t y, int16_t h, uint8_t color);
void ST7920_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
void ST7920_DrawBox(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
void ST7920_DrawFrame(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
void ST7920_DrawRFrame(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t color);
void ST7920_DrawRBox(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t color);
void ST7920_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color);
void ST7920_DrawDisc(int16_t x0, int16_t y0, int16_t r, uint8_t color);
void ST7920_DrawTriangle(int16_t x0, int16_t y0,
                         int16_t x1, int16_t y1,
                         int16_t x2, int16_t y2,
                         uint8_t color);

/* Text */
void ST7920_DrawChar(int16_t x, int16_t y, char c);
void ST7920_DrawStr(int16_t x, int16_t y, const char *str);

/* Bitmap */
void ST7920_DrawXBMP(int16_t x, int16_t y, int16_t w, int16_t h,
                     const uint8_t *bitmap);

#endif /* ST7920_H */
