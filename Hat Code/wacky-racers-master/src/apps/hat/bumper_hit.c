#include <stdbool.h>
#include "pio.h"
#include "pwm.h"
#include "target.h"
#include "bumper_hit.h"
#include "ledbuffer.h"

// sad trombone — Bb4, A4, Ab4, Eb4 (each note repeated for duration)
static const uint16_t jingle[] = {466, 466, 440, 440, 415, 415, 311, 311, 311, 311};
#define NUM_NOTES (sizeof(jingle) / sizeof(jingle[0]))
#define NOTE_TICKS   15     // ticks per note step
#define BUMPER_DURATION_TICKS (NUM_NOTES * NOTE_TICKS)

static int ticks = 0;
static bool active = false;
static pwm_t buzzer_pwm;

bool bumper_hit_is_active(void)
{
    return active;
}

void bumper_hit_start(void)
{
    if (!buzzer_pwm)
        buzzer_pwm = pwm_init(&(pwm_cfg_t){.pio = BUZZER_PIO, .frequency = 466, .duty_ppt = 500});
    ticks = 0;
    active = true;
    pwm_start(buzzer_pwm);
}

void bumper_hit_stop(void)
{
    active = false;
    pwm_stop(buzzer_pwm);
}

void bumper_hit_update(ledbuffer_t *leds)
{
    if (!active)
        return;

    // step through jingle notes
    int note_index = (ticks / NOTE_TICKS) % NUM_NOTES;
    pwm_frequency_set(buzzer_pwm, jingle[note_index]);
    pwm_duty_ppt_set(buzzer_pwm, 500);

    // flash all LEDs red, blink every 10 ticks
    ledbuffer_clear(leds);
    if ((ticks / 10) % 2 == 0)
    {
        for (int i = 0; i < NUM_LEDS; i++)
            ledbuffer_set(leds, i, 255, 0, 0);
    }
    ledbuffer_write(leds);

    ticks++;
    if (ticks >= BUMPER_DURATION_TICKS)
        bumper_hit_stop();
}
