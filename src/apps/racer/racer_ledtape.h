#ifndef RACER_LEDTAPE_H
#define RACER_LEDTAPE_H

#include <stdbool.h>
#include <stdint.h>

#include "button.h"
#include "ledbuffer.h"

typedef enum
{
    RACER_LEDTAPE_MODE_RAINBOW,
    RACER_LEDTAPE_MODE_GREEN,
    RACER_LEDTAPE_MODE_RED,
    RACER_LEDTAPE_MODE_BLUE,
    RACER_LEDTAPE_MODE_BLOCKS,
    RACER_LEDTAPE_MODE_OFF,
    RACER_LEDTAPE_MODE_NUM
} racer_ledtape_mode_t;

typedef struct
{
    ledbuffer_t *leds;
    button_t pattern_button;
    bool enabled;
    racer_ledtape_mode_t mode;
    uint8_t rainbow_offset;
    uint8_t block_filled;
    uint8_t block_pos;
    uint8_t block_colour;
    uint8_t block_ticks;
    uint8_t celebration_ticks;
    uint8_t write_ticks;
    uint16_t bumper_ticks;
    bool bumper_active;
} racer_ledtape_t;

int racer_ledtape_init(racer_ledtape_t *ledtape);
void racer_ledtape_set(racer_ledtape_t *ledtape, bool enabled);
void racer_ledtape_bumper_set(racer_ledtape_t *ledtape, bool active);
void racer_ledtape_update(racer_ledtape_t *ledtape);

#endif
