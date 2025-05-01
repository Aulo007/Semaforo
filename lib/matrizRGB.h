#ifndef MATRIZRGB_H
#define MATRIZRGB_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

#define LED_COUNT 25

typedef struct {
    uint8_t G, R, B;
} npLED_t;

typedef struct {
    uint8_t r, g, b;
} npColor_t;

// Cores predefinidas
#define COLOR_BLACK  (npColor_t){0, 0, 0}
#define COLOR_RED     (npColor_t){255, 0, 0}
#define COLOR_GREEN   (npColor_t){0, 255, 0}
#define COLOR_BLUE    (npColor_t){0, 0, 255}
#define COLOR_WHITE   (npColor_t){255, 255, 255}
#define COLOR_YELLOW  (npColor_t){255, 170, 0}
#define COLOR_CYAN    (npColor_t){0, 255, 255}
#define COLOR_MAGENTA (npColor_t){255, 0, 255}
#define COLOR_PURPLE  (npColor_t){128, 0, 128}
#define COLOR_ORANGE  (npColor_t){255, 20, 0}
#define COLOR_BROWN   (npColor_t){60, 40, 0}
#define COLOR_VIOLET  (npColor_t){175, 0, 168}
#define COLOR_GREY    (npColor_t){128, 128, 128}
#define COLOR_GOLD    (npColor_t){255, 215, 0}
#define COLOR_SILVER  (npColor_t){192, 192, 192}

extern npColor_t npColors[];

// Protótipos das funções
void npInit(uint8_t pin);
void npClear();
void npWrite();
void npSetMatrixWithIntensity(npColor_t matrix[5][5], float intensity);
void npAnimateFrames(int period, int num_frames, npColor_t frames[num_frames][5][5], float intensity);
void npSetLED(int x, int y, npColor_t color);
void npSetLEDIntensity(int x, int y, npColor_t color, float intensity);
void npSetRow(int row, npColor_t color);
void npSetRowIntensity(int row, npColor_t color, float intensity);
void npSetColumn(int col, npColor_t color);;
void npSetColumnItensity(int col, npColor_t color, float intensity);
void npSetBorder(npColor_t color);
void npSetDiagonal(bool mainDiagonal, npColor_t color);
void npFill(npColor_t color);
void npFillIntensity(npColor_t color, float intensity);
bool npIsPositionValid(int x, int y);

extern npLED_t leds[LED_COUNT];

#endif