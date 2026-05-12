#ifndef PWM_SEQUENCE_H
#define PWM_SEQUENCE_H

typedef struct
{
    int value;
} pwm_sequence_t;

void pwm_sequence_init(pwm_sequence_t *sequence);
int pwm_sequence_next(pwm_sequence_t *sequence);

#endif
