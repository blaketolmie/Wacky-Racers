#ifndef RACER_BUMPER_H
#define RACER_BUMPER_H

#include <stdbool.h>
#include <stdint.h>

#include "button.h"

typedef struct
{
    button_t button;
    uint16_t hbridge_off_ticks;
} racer_bumper_t;

int racer_bumper_init(racer_bumper_t *bumper);
bool racer_bumper_update(racer_bumper_t *bumper);
void racer_bumper_reset(racer_bumper_t *bumper);

#endif
