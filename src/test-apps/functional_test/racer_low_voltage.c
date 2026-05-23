#include "racer_low_voltage.h"
#include "pio.h"
#include "target.h"

#define LOW_VOLTAGE_LED_PIO LED_ERROR_PIO

static void low_voltage_led_set(bool on)
{
    pio_output_set(LOW_VOLTAGE_LED_PIO, on ? LED_ACTIVE : !LED_ACTIVE);
}

void racer_low_voltage_init(void)
{
    pio_config_set(BATTERY_MONITOR_PIO, PIO_PULLDOWN);
    pio_config_set(LOW_VOLTAGE_LED_PIO, PIO_OUTPUT_HIGH);
    low_voltage_led_set(false);
}

bool racer_low_voltage_update(void)
{
    bool low_voltage;

    low_voltage = ! pio_input_get(BATTERY_MONITOR_PIO);
    low_voltage_led_set(low_voltage);

    return low_voltage;
}
