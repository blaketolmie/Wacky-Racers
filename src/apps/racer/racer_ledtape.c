#include <stdio.h>

#include "racer_ledtape.h"
#include "target.h"

#define LEDTAPE_PATTERN_BUTTON_PIO BUTTON_PIO2
#define LEDTAPE_WRITE_TICKS 4
#define BLOCK_STEP_TICKS 6
#define CELEBRATION_TICKS 14
#define BUMPER_FLASH_TICKS 10

static const button_cfg_t ledtape_button_cfg =
{
    .pio = LEDTAPE_PATTERN_BUTTON_PIO
};

static const char *ledtape_mode_names[] =
{
    "rainbow",
    "green",
    "red",
    "blue",
    "blocks",
    "off"
};

static void rainbow_colour(uint8_t wheel_pos,
                           uint8_t *red, uint8_t *green, uint8_t *blue)
{
    if (wheel_pos < 85)
    {
        *red = 255 - wheel_pos * 3;
        *green = wheel_pos * 3;
        *blue = 0;
        return;
    }

    if (wheel_pos < 170)
    {
        wheel_pos -= 85;
        *red = 0;
        *green = 255 - wheel_pos * 3;
        *blue = wheel_pos * 3;
        return;
    }

    wheel_pos -= 170;
    *red = wheel_pos * 3;
    *green = 0;
    *blue = 255 - wheel_pos * 3;
}

static void ledtape_fill_solid(ledbuffer_t *leds,
                               uint8_t red, uint8_t green, uint8_t blue)
{
    int i;

    ledbuffer_clear(leds);
    for (i = 0; i < LED_STRIP_NUMBER; i++)
        ledbuffer_set(leds, i, red, green, blue);
}

static void ledtape_fill_bumper(racer_ledtape_t *ledtape)
{
    int i;

    /*
       Match the hat bumper pattern: all LEDs flash bright red, with a
       10-tick on/off blink while the bumper stop window is active.
    */
    ledbuffer_clear(ledtape->leds);
    if ((ledtape->bumper_ticks / BUMPER_FLASH_TICKS) % 2 == 0)
    {
        for (i = 0; i < LED_STRIP_NUMBER; i++)
            ledbuffer_set(ledtape->leds, i, 255, 0, 0);
    }
}

static void ledtape_fill_rainbow(racer_ledtape_t *ledtape)
{
    int i;

    for (i = 0; i < LED_STRIP_NUMBER; i++)
    {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t wheel_pos;

        /* Spread the colours out so every LED is doing something different. */
        wheel_pos = ledtape->rainbow_offset + i * (256 / LED_STRIP_NUMBER);
        rainbow_colour(wheel_pos, &red, &green, &blue);
        ledbuffer_set(ledtape->leds, i, red, green, blue);
    }
}

static void block_colour(uint8_t colour,
                         uint8_t *red, uint8_t *green, uint8_t *blue)
{
    switch (colour % 5)
    {
    case 0:
        *red = 60;
        *green = 0;
        *blue = 0;
        break;

    case 1:
        *red = 0;
        *green = 45;
        *blue = 0;
        break;

    case 2:
        *red = 0;
        *green = 0;
        *blue = 60;
        break;

    case 3:
        *red = 45;
        *green = 45;
        *blue = 0;
        break;

    default:
        *red = 45;
        *green = 0;
        *blue = 45;
        break;
    }
}

static void ledtape_blocks_reset(racer_ledtape_t *ledtape)
{
    ledtape->block_filled = 0;
    ledtape->block_pos = 0;
    ledtape->block_colour = 0;
    ledtape->block_ticks = 0;
    ledtape->celebration_ticks = 0;
}

static void ledtape_fill_blocks(racer_ledtape_t *ledtape)
{
    int i;

    ledbuffer_clear(ledtape->leds);

    if (ledtape->celebration_ticks > 0)
    {
        ledtape_fill_rainbow(ledtape);
        return;
    }

    for (i = 0; i < ledtape->block_filled; i++)
    {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t led_index;

        led_index = LED_STRIP_NUMBER - 1 - i;
        block_colour(i, &red, &green, &blue);
        ledbuffer_set(ledtape->leds, led_index, red, green, blue);
    }

    if (ledtape->block_filled < LED_STRIP_NUMBER)
    {
        uint8_t red;
        uint8_t green;
        uint8_t blue;

        block_colour(ledtape->block_colour, &red, &green, &blue);
        ledbuffer_set(ledtape->leds, ledtape->block_pos, red, green, blue);
    }
}

static void ledtape_blocks_step(racer_ledtape_t *ledtape)
{
    uint8_t target_pos;

    if (ledtape->celebration_ticks > 0)
    {
        ledtape->celebration_ticks--;
        ledtape->rainbow_offset += 12;
        if (ledtape->celebration_ticks == 0)
            ledtape_blocks_reset(ledtape);

        ledtape_fill_blocks(ledtape);
        return;
    }

    if (ledtape->block_filled >= LED_STRIP_NUMBER)
    {
        ledtape->celebration_ticks = CELEBRATION_TICKS;
        ledtape->rainbow_offset = 0;
        ledtape_fill_blocks(ledtape);
        return;
    }

    target_pos = LED_STRIP_NUMBER - 1 - ledtape->block_filled;
    if (ledtape->block_pos < target_pos)
    {
        ledtape->block_pos++;
    }
    else
    {
        ledtape->block_filled++;
        ledtape->block_colour++;
        ledtape->block_pos = 0;

        if (ledtape->block_filled >= LED_STRIP_NUMBER)
        {
            ledtape->celebration_ticks = CELEBRATION_TICKS;
            ledtape->rainbow_offset = 0;
        }
    }

    ledtape_fill_blocks(ledtape);
}

static void ledtape_show_mode(racer_ledtape_t *ledtape)
{
    if (! ledtape->enabled)
    {
        ledbuffer_clear(ledtape->leds);
        return;
    }

    switch (ledtape->mode)
    {
    case RACER_LEDTAPE_MODE_GREEN:
        /* Soft green is the normal awake colour. */
        ledtape_fill_solid(ledtape->leds, 0, 40, 0);
        break;

    case RACER_LEDTAPE_MODE_RED:
        ledtape_fill_solid(ledtape->leds, 40, 0, 0);
        break;

    case RACER_LEDTAPE_MODE_BLUE:
        ledtape_fill_solid(ledtape->leds, 0, 0, 40);
        break;

    case RACER_LEDTAPE_MODE_RAINBOW:
        ledtape_fill_rainbow(ledtape);
        break;

    case RACER_LEDTAPE_MODE_BLOCKS:
        ledtape_fill_blocks(ledtape);
        break;

    case RACER_LEDTAPE_MODE_OFF:
    default:
        ledbuffer_clear(ledtape->leds);
        break;
    }
}

int racer_ledtape_init(racer_ledtape_t *ledtape)
{
    ledtape->leds = ledbuffer_init(LEDTAPE_PIO, LED_STRIP_NUMBER);
    if (! ledtape->leds)
        return 1;

    ledtape->pattern_button = button_init(&ledtape_button_cfg);
    if (! ledtape->pattern_button)
        return 2;

    ledtape->enabled = true;
    ledtape->mode = RACER_LEDTAPE_MODE_RAINBOW;
    ledtape->rainbow_offset = 0;
    ledtape_blocks_reset(ledtape);
    ledtape->write_ticks = 0;
    ledtape->bumper_ticks = 0;
    ledtape->bumper_active = false;

    ledtape_show_mode(ledtape);
    ledbuffer_write(ledtape->leds);

    return 0;
}

void racer_ledtape_set(racer_ledtape_t *ledtape, bool enabled)
{
    ledtape->enabled = enabled;

    ledtape_show_mode(ledtape);
    ledbuffer_write(ledtape->leds);
}

void racer_ledtape_bumper_set(racer_ledtape_t *ledtape, bool active)
{
    if (active && ! ledtape->bumper_active)
        ledtape->bumper_ticks = 0;

    if (! active && ledtape->bumper_active)
    {
        ledtape->bumper_ticks = 0;
        ledtape->write_ticks = 0;
        ledtape_show_mode(ledtape);
        ledbuffer_write(ledtape->leds);
    }

    ledtape->bumper_active = active;
}

void racer_ledtape_update(racer_ledtape_t *ledtape)
{
    if (ledtape->bumper_active)
    {
        ledtape_fill_bumper(ledtape);
        ledbuffer_write(ledtape->leds);
        ledtape->bumper_ticks++;
        return;
    }

    button_poll(ledtape->pattern_button);

    if (button_pushed_p(ledtape->pattern_button))
    {
        ledtape->mode++;
        if (ledtape->mode >= RACER_LEDTAPE_MODE_NUM)
            ledtape->mode = RACER_LEDTAPE_MODE_RAINBOW;

        if (ledtape->mode == RACER_LEDTAPE_MODE_RAINBOW)
            ledtape->rainbow_offset = 0;
        else if (ledtape->mode == RACER_LEDTAPE_MODE_BLOCKS)
            ledtape_blocks_reset(ledtape);

        ledtape_show_mode(ledtape);
        ledbuffer_write(ledtape->leds);

        printf("LED tape mode: %s\r\n", ledtape_mode_names[ledtape->mode]);
        fflush(stdout);
    }

    if (! ledtape->enabled)
        return;

    ledtape->write_ticks++;
    if (ledtape->write_ticks >= LEDTAPE_WRITE_TICKS)
    {
        ledtape->write_ticks = 0;

        if (ledtape->mode == RACER_LEDTAPE_MODE_RAINBOW)
        {
            ledtape->rainbow_offset += 4;
            ledtape_fill_rainbow(ledtape);
        }
        else if (ledtape->mode == RACER_LEDTAPE_MODE_BLOCKS)
        {
            ledtape->block_ticks++;
            if (ledtape->block_ticks >= BLOCK_STEP_TICKS)
            {
                ledtape->block_ticks = 0;
                ledtape_blocks_step(ledtape);
            }
        }

        ledbuffer_write(ledtape->leds);
    }
}
