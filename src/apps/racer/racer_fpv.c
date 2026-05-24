/*
   FPV camera power control.

   BUTTON_PIO toggles whether the FPV output is enabled.  The state is stored
   in racer_fpv_t so sleep mode can temporarily turn power off, then restore
   the user's chosen FPV setting after wakeup.
*/
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
    /* Apply the remembered state to the actual power-enable pin. */
    if (fpv->enabled)
        pio_output_high(FPV_ENABLE_PIO);
    else
        pio_output_low(FPV_ENABLE_PIO);
}

int racer_fpv_init(racer_fpv_t *fpv)
{
    /* Create a debounced button object for the FPV toggle button. */
    fpv->button = button_init(&fpv_button_cfg);
    if (! fpv->button)
        return 1;

    /* Start with FPV on unless the user turns it off. */
    fpv->enabled = true;
    racer_fpv_apply(fpv);

    return 0;
}

void racer_fpv_update(racer_fpv_t *fpv)
{
    /* Poll once per main-loop tick so the button library can debounce it. */
    button_poll(fpv->button);

    if (button_pushed_p(fpv->button))
    {
        /* Toggle on the press edge, not continuously while held. */
        fpv->enabled = !fpv->enabled;
        racer_fpv_apply(fpv);

        printf("FPV %s using BUTTON_PIO\r\n", fpv->enabled ? "ON" : "OFF");
        fflush(stdout);
    }
}
