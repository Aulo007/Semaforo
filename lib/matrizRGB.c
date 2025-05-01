#include "matrizRGB.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2818b.pio.h"

npLED_t leds[LED_COUNT];
static PIO np_pio;
static uint sm;

npColor_t npColors[] = {
    COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_WHITE, COLOR_BLACK,
    COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_PURPLE, COLOR_ORANGE,
    COLOR_BROWN, COLOR_VIOLET, COLOR_GREY, COLOR_GOLD, COLOR_SILVER};

void npInit(uint8_t pin)
{
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, false);

    if (sm < 0)
    {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }

    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    npClear();
}

void npWrite()
{
    for (uint i = 0; i < LED_COUNT; ++i)
    {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
}

void npClear()
{
    for (uint i = 0; i < LED_COUNT; ++i)
    {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
    npWrite();
}

int getIndex(int x, int y)
{
    if (y % 2 == 0)
        return 24 - (y * 5 + x);
    return 24 - (y * 5 + (4 - x));
}

bool npIsPositionValid(int x, int y)
{
    return (x >= 0 && x < 5 && y >= 0 && y < 5);
}

void npSetLED(int x, int y, npColor_t color)
{
    if (npIsPositionValid(x, y))
    {
        uint index = getIndex(x, y);
        leds[index].R = color.r;
        leds[index].G = color.g;
        leds[index].B = color.b;
    }
}

void npSetLEDIntensity(int x, int y, npColor_t color, float intensity)
{
    if (npIsPositionValid(x, y))
    {
        intensity = (intensity < 0.0f) ? 0.0f : (intensity > 1.0f) ? 1.0f
                                                                   : intensity;
        uint index = getIndex(x, y);
        leds[index].R = (uint8_t)(color.r * intensity);
        leds[index].G = (uint8_t)(color.g * intensity);
        leds[index].B = (uint8_t)(color.b * intensity);
    }
}

void npSetRow(int row, npColor_t color)
{
    if (row >= 0 && row < 5)
    {
        for (int x = 0; x < 5; x++)
        {
            npSetLED(x, row, color);
        }
        npWrite();
    }
}

void npSetColumn(int col, npColor_t color)
{
    if (col >= 0 && col < 5)
    {
        for (int y = 0; y < 5; y++)
        {
            npSetLED(col, y, color);
        }
        npWrite();
    }
}

void npSetBorder(npColor_t color)
{
    for (int x = 0; x < 5; x++)
    {
        npSetLED(x, 0, color);
        npSetLED(x, 4, color);
    }
    for (int y = 1; y < 4; y++)
    {
        npSetLED(0, y, color);
        npSetLED(4, y, color);
    }
    npWrite();
}

void npSetDiagonal(bool mainDiagonal, npColor_t color)
{
    for (int i = 0; i < 5; i++)
    {
        if (mainDiagonal)
        {
            npSetLED(i, i, color);
        }
        else
        {
            npSetLED(4 - i, i, color);
        }
    }
    npWrite();
}

void npFill(npColor_t color)
{
    for (int i = 0; i < LED_COUNT; i++)
    {
        leds[i].R = color.r;
        leds[i].G = color.g;
        leds[i].B = color.b;
    }
    npWrite();
}

void npFillIntensity(npColor_t color, float intensity)
{
    intensity = (intensity < 0.0f) ? 0.0f : (intensity > 1.0f) ? 1.0f
                                                               : intensity;
    for (int i = 0; i < LED_COUNT; i++)
    {
        leds[i].R = (uint8_t)(color.r * intensity);
        leds[i].G = (uint8_t)(color.g * intensity);
        leds[i].B = (uint8_t)(color.b * intensity);
    }
    npWrite();
}

void npSetMatrixWithIntensity(npColor_t matrix[5][5], float intensity)
{
    intensity = (intensity < 0.0f) ? 0.0f : (intensity > 1.0f) ? 1.0f
                                                               : intensity;
    for (int y = 0; y < 5; y++)
    {
        for (int x = 0; x < 5; x++)
        {
            uint index = getIndex(x, y);
            leds[index].R = (uint8_t)(matrix[y][x].r * intensity);
            leds[index].G = (uint8_t)(matrix[y][x].g * intensity);
            leds[index].B = (uint8_t)(matrix[y][x].b * intensity);
        }
    }
    npWrite();
}

void npAnimateFrames(int period, int num_frames, npColor_t frames[num_frames][5][5], float intensity)
{
    for (int i = 0; i < num_frames; i++)
    {
        npSetMatrixWithIntensity(frames[i], intensity);
        sleep_ms(period);
    }
}

void npSetColumnItensity(int col, npColor_t color, float intensity)
{
    if (col >= 0 && col < 5)
    {
        for (int y = 0; y < 5; y++)
        {
            npSetLEDIntensity(col, y, color, intensity);
        }
        npWrite();
    }
}
void npSetRowIntensity(int row, npColor_t color, float intensity)
{
    color.r = (uint8_t)(color.r * intensity);
    color.g = (uint8_t)(color.g * intensity);
    color.b = (uint8_t)(color.b * intensity);
    npSetRow(row, color);
}
