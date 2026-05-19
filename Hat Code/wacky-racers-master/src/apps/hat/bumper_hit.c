#include "pio.h"
#include "pwm.h"
#include "target.h"
#include "bumper_hit.h"

#define BUMPER_HIT_DURATION_TICKS 3  // 2 seconds

static int ticks_remaining = 0;
static pwm_t buzzer_pwm;

void bumper_hit_start(void)
{
    if (!buzzer_pwm)
        buzzer_pwm = pwm_init(&(pwm_cfg_t){.pio = BUZZER_PIO, .frequency = 1000, .duty_ppt = 500});
    pwm_start(buzzer_pwm);
    ticks_remaining = BUMPER_HIT_DURATION_TICKS;
    pio_config_set(LED_RED_PIO, LED_ACTIVE);
}

void bumper_hit_stop(void)
{
    pio_config_set(LED_RED_PIO, !LED_ACTIVE);
    pwm_stop(buzzer_pwm);
}

// Call once per pacer tick. Returns 1 while active, 0 when done.
int bumper_hit_update(void)
{
    // if (ticks_remaining == 0)
    // {
    //     return 0;
    // }

    pio_output_toggle(LED_RED_PIO);

    if (ticks_remaining % (PACER_RATE / 4) < (PACER_RATE / 8))
    {
        pwm_duty_ppt_set(buzzer_pwm, 100);
    }
    else
    {
        pwm_duty_ppt_set(buzzer_pwm, 500);
    }

    ticks_remaining--;

    if (ticks_remaining == 0)
    {
        pio_config_set(LED_RED_PIO, !LED_ACTIVE);
        pwm_stop(buzzer_pwm);
    }

    return ticks_remaining; //> 0 ? 1 : 0;
}
