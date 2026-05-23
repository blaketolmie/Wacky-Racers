#include <stdio.h>

#include "racer_fpv.h"
#include "pio.h"
#include "target.h"

#define FPV_BUTTON_PIO BUTTON_PIO

static const button_cfg_t fpv_button_cfg =
{
    .pio = FPV_BUTTON_PIO
};

void racer_fpv_apply(racer_fpv_t *fpv)
{
    if (fpv->enabled)
        pio_output_high(FPV_ENABLE_PIO);
    else
        pio_output_low(FPV_ENABLE_PIO);
}

int racer_fpv_init(racer_fpv_t *fpv)
{
    fpv->button = button_init(&fpv_button_cfg);
    if (! fpv->button)
        return 1;

    fpv->enabled = true;
    racer_fpv_apply(fpv);

    return 0;
}

void racer_fpv_update(racer_fpv_t *fpv)
{
    button_poll(fpv->button);

    if (button_pushed_p(fpv->button))
    {
        fpv->enabled = !fpv->enabled;
        racer_fpv_apply(fpv);

        printf("FPV %s using BUTTON_PIO\r\n", fpv->enabled ? "ON" : "OFF");
        fflush(stdout);
    }
}
