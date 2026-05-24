/*
   Board-level power control for the racer.

   This module owns the simple enable pins for the H-bridge, FPV output,
   radio, IMU, and board LEDs.  Sleep mode calls this file to turn outputs
   off before WAIT sleep and restore them afterwards.
*/
#include "racer_power.h"
#include "pio.h"
#include "delay.h"
#include "target.h"

static void outputs_config(void)
{
    /* Configure every controlled output into a known initial state. */
    pio_config_set(HBRIDGE_ENABLE_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(FPV_ENABLE_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(RADIO_OFF_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(IMU_ENABLE_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(LED_STATUS_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(LED_ERROR_PIO, PIO_OUTPUT_HIGH);
}

static void board_leds_set(bool on)
{
    /* Both board LEDs are active-low, so LED_ACTIVE means "on". */
    pio_output_set(LED_STATUS_PIO, on ? LED_ACTIVE : !LED_ACTIVE);
    pio_output_set(LED_ERROR_PIO, on ? LED_ACTIVE : !LED_ACTIVE);
}

void racer_power_init(void)
{
    /* Set pin directions, then apply the normal awake state. */
    outputs_config();
    racer_power_sleep_exit();
}

void racer_power_sleep_enter(void)
{
    /* Shut down power-hungry outputs before the MCU enters WAIT mode. */
    board_leds_set(false);
    pio_output_low(HBRIDGE_ENABLE_PIO);
    pio_output_low(FPV_ENABLE_PIO);
    pio_output_low(RADIO_OFF_PIO);
    pio_output_low(IMU_ENABLE_PIO);
}

void racer_power_sleep_exit(void)
{
    /* Restore the outputs needed for normal driving after wakeup. */
    board_leds_set(false);

    pio_output_high(HBRIDGE_ENABLE_PIO);
    pio_output_high(RADIO_OFF_PIO);
    pio_output_high(IMU_ENABLE_PIO);

    /* Give the radio regulator and IMU enable a moment to settle. */
    delay_ms(10);
}
