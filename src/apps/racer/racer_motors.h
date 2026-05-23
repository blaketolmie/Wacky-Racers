#ifndef RACER_MOTORS_H
#define RACER_MOTORS_H

#include "pwm.h"

typedef struct
{
    pwm_t left_pwm;
    pwm_t right_pwm;
} racer_motors_t;

int racer_motors_init(racer_motors_t *motors);
void racer_motors_stop(racer_motors_t *motors);
void racer_motors_set(racer_motors_t *motors,
                      int left_command, int right_command);

#endif
