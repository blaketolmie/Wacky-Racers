#include <pio.h>
#include "low_voltage.h"
#include "adc.h"
#include "panic.h"
#include "target.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>


static const adc_cfg_t adc_cfg =
{
    .bits = 12,
    .channels = BIT (ADC_CHANNEL_1),
    .trigger = ADC_TRIGGER_SW,
    .clock_speed_kHz = 1000
};

static adc_t adc;

void init_low_voltage(void)
{
    pio_config_set(LED_RED_PIO, PIO_OUTPUT_LOW);
    adc = adc_init (&adc_cfg);
    if (! adc)
        panic (LED_ERROR_PIO, 1);
    
}

int get_battery_voltage(void)
{
    uint16_t data[1];
    adc_read(adc, data, sizeof(data));
    int adc_voltage = (data[0] * 3300 / 4096) * 2;
    /* printf("mV: %d\n\r", adc_voltage); */
    return adc_voltage;
}

// if TRUE enables low power, if FALSE disables low power
void low_power(bool STATE)
{
    if (STATE == true)
    {
        pio_output_toggle(LED_RED_PIO);
        // printf("LOW_POWER");

    }
    if (STATE == false)
    {
        pio_output_set(LED_RED_PIO, !LED_ACTIVE);
        // printf("NORMAL");
    }
}
