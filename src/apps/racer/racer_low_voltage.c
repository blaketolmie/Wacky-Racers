#include "racer_low_voltage.h"
#include "mcu.h"
#include "pio.h"
#include "target.h"

#define LOW_VOLTAGE_LED_PIO LED_ERROR_PIO
#define LOW_VOLTAGE_ACTIVE 0

static void low_voltage_led_set(bool on)
{
    pio_output_set(LOW_VOLTAGE_LED_PIO, on ? LED_ACTIVE : !LED_ACTIVE);
}

void racer_low_voltage_init(void)
{
    /*
       BATTERY_MONITOR_PIO is PB5 on the racer board.  PB5 defaults to the
       JTAG/TDO function on the SAM4S, so make it available as a normal input.
    */
    mcu_jtag_disable();

    /*
       The monitor signal is active-low.  Use the pull-up so an open or
       open-drain monitor reads as normal instead of permanently low.
    */
    pio_config_set(BATTERY_MONITOR_PIO, PIO_PULLUP);
#ifdef PGOOD_PIO
    pio_config_set(PGOOD_PIO, PIO_PULLUP);
#endif
    pio_config_set(LOW_VOLTAGE_LED_PIO, PIO_OUTPUT_HIGH);
    low_voltage_led_set(false);
}

bool racer_low_voltage_update(void)
{
    bool low_voltage;

    low_voltage = pio_input_get(BATTERY_MONITOR_PIO) == LOW_VOLTAGE_ACTIVE;
#ifdef PGOOD_PIO
    low_voltage = low_voltage || ! pio_input_get(PGOOD_PIO);
#endif
    low_voltage_led_set(low_voltage);

    return low_voltage;
}
