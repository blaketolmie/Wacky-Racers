#ifndef RACER_SLEEP_H
#define RACER_SLEEP_H

#include <stdbool.h>

#include "button.h"

typedef struct
{
    button_t button;
    bool sleeping;
} racer_sleep_t;

int racer_sleep_init(racer_sleep_t *sleep);
void racer_sleep_poll(racer_sleep_t *sleep);
bool racer_sleep_toggle_requested_p(racer_sleep_t *sleep);
void racer_sleep_arm(racer_sleep_t *sleep);
void racer_sleep_wait_for_wake(racer_sleep_t *sleep);
void racer_sleep_finish(racer_sleep_t *sleep);

#endif
