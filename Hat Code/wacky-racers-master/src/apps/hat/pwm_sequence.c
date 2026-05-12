#include "pwm_sequence.h"

#define PWM_START 100
#define PWM_END -100
#define PWM_STEP 1

void pwm_sequence_init(pwm_sequence_t *sequence)
{
    sequence->value = PWM_START;
}

int pwm_sequence_next(pwm_sequence_t *sequence)
{
    int value;

    value = sequence->value;
    sequence->value -= PWM_STEP;

    if (sequence->value < PWM_END)
        sequence->value = PWM_START;

    return value;
}
