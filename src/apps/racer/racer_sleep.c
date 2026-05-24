/*
   Sleep handling for the racer.

   This module uses SAM4S WAIT mode.  WAIT mode is lighter than fully turning
   the MCU off: the CPU stops to save power, but SRAM and program variables
   stay alive.  When the sleep button is pressed again, the program continues
   from where it went to sleep.
*/
#include <stdio.h>

/* Public sleep module interface used by the main racer test app. */
#include "racer_sleep.h"
/* Kept because the commented BACKUP-mode example below uses mcu_sleep(). */
#include "mcu_sleep.h"
/* Used for the small button-release debounce waits. */
#include "delay.h"
/* Gives this file the racer board pin names, like SLEEP_PIO. */
#include "target.h"

/* The physical sleep pushbutton on the racer board.  This is PA2/WKUP2. */
#define SLEEP_BUTTON_PIO SLEEP_PIO

/* Extra time after button release so contact bounce does not retrigger sleep. */
#define BUTTON_RELEASE_DEBOUNCE_MS 50

/* Bits 0..18 in PMC_FSMR are the fast-startup enable bits and alarm enables. */
#define WAIT_WAKEUP_FAST_STARTUP_MASK 0x0007ffffu

/* Bits 0..15 in PMC_FSPR choose the fast-startup input polarities. */
#define WAIT_WAKEUP_POLARITY_MASK 0x0000ffffu

/* WKUP2 maps to fast-startup input 2, because SLEEP_PIO is PA2/WKUP2. */
#define WAIT_WAKEUP2_ENABLE PMC_FSMR_FSTT2

/* For this active-low button, keep FSTP2 clear so a low level wakes the MCU. */
#define WAIT_WAKEUP2_ACTIVE_LOW 0

/* PMC_FSMR bit 20 enables the low-power WAIT mode path. */
#define WAIT_MODE_LOW_POWER BIT(20)

/* Configuration for the normal debounced button driver. */
static const button_cfg_t sleep_button_cfg =
{
    /* The button driver assumes a pushbutton pulls the input low. */
    .pio = SLEEP_BUTTON_PIO
};

/*
   This file uses SAM4S WAIT mode, not BACKUP mode.

   WAIT mode:
       - stops the CPU/core clocks to save power,
       - keeps SRAM and program state alive,
       - wakes from a fast-startup input,
       - continues from the line after the WAIT command.

   PA2 is WKUP2 on the SAM4S, so this file enables fast-startup input 2.

   If you want deeper BACKUP mode later, you can use code like this:

       static const mcu_sleep_wakeup_t backup_wakeups[] =
       {
           {
               .pio = SLEEP_BUTTON_PIO,
               .active_high = false
           }
       };

       static const mcu_sleep_cfg_t backup_sleep_cfg =
       {
           .mode = MCU_SLEEP_MODE_BACKUP,
           .debounce = 0,
           .num_wakeups = 1,
           .wakeups = backup_wakeups
       };

       mcu_sleep(&backup_sleep_cfg);

   BACKUP mode wakes using WKUP2 too, but it restarts the MCU like
   pressing nRST, so main() starts again from the beginning.
*/
static void wait_mode_wkup2_sleep(void)
{
    /*
       CKGR_MOR contains protected clock-control bits.  The SAM4S requires a
       key when writing it, so this temporary stores the old value without
       the old key bits before adding WAITMODE.
    */
    uint32_t mor;

    /*
       Do not enter WAIT if the wake button is already held low.  If we slept
       while the wake condition was already active, the MCU could wake
       immediately and make the sleep button feel like a hold-to-sleep button.
    */
    if (! pio_input_get(SLEEP_BUTTON_PIO))
        return;

    /*
       Enable WKUP2 in the Supply Controller too.  WAIT mode mainly needs the
       PMC fast-startup setup below, but keeping SUPC configured makes the
       pin setup match the BACKUP-mode wakeup path shown above.
    */
    SUPC->SUPC_WUIR = SUPC_WUIR_WKUPEN2;

    /*
       Enable the low-power WAIT path and fast-startup input 2.
       The mask clears any old fast-startup/alarm source bits first, then the
       code turns on only the WKUP2 source used by the sleep button.
    */
    PMC->PMC_FSMR =
        (PMC->PMC_FSMR & ~WAIT_WAKEUP_FAST_STARTUP_MASK)
        | WAIT_MODE_LOW_POWER | WAIT_WAKEUP2_ENABLE;

    /*
       Choose the active level for WKUP2.  The sleep button is active-low, so
       FSTP2 is left as 0 and a low level on PA2 wakes the MCU.
    */
    PMC->PMC_FSPR =
        (PMC->PMC_FSPR & ~WAIT_WAKEUP_POLARITY_MASK) | WAIT_WAKEUP2_ACTIVE_LOW;

    /*
       Preserve the current oscillator settings, remove the old password
       field, then write the required key and WAITMODE command bit.
    */
    mor = PMC->CKGR_MOR & ~CKGR_MOR_KEY_Msk;
    PMC->CKGR_MOR = mor | CKGR_MOR_KEY(0x37) | CKGR_MOR_WAITMODE;

    /*
       When WAIT mode wakes, wait until the master clock is ready before the
       rest of the program turns outputs and peripherals back on.
    */
    while (! (PMC->PMC_SR & PMC_SR_MCKRDY))
        ;
}

/* Wait until the active-low sleep button is released, then debounce it. */
static void sleep_button_release_wait(void)
{
    /* While the input reads low, the button is still physically pressed. */
    while (! pio_input_get(SLEEP_BUTTON_PIO))
        delay_ms(5);

    /* Give the switch contacts time to settle after release. */
    delay_ms(BUTTON_RELEASE_DEBOUNCE_MS);
}

/* Set up the sleep module state and create the debounced button object. */
int racer_sleep_init(racer_sleep_t *sleep)
{
    /* The app starts awake. */
    sleep->sleeping = false;

    /* Register SLEEP_PIO with the button driver. */
    sleep->button = button_init(&sleep_button_cfg);

    /* Return an error if the button driver ran out of button slots. */
    if (! sleep->button)
        return 1;

    /* Zero means success, matching the other racer modules. */
    return 0;
}

/* Poll the debounced button state once per main-loop tick. */
void racer_sleep_poll(racer_sleep_t *sleep)
{
    button_poll(sleep->button);
}

/* True only on the debounced press edge, not while the button is held. */
bool racer_sleep_toggle_requested_p(racer_sleep_t *sleep)
{
    return button_pushed_p(sleep->button);
}

/*
   First half of entering sleep.
   The main program calls this before it turns outputs off, so the board does
   not look asleep while the first press is still being held.
*/
void racer_sleep_arm(racer_sleep_t *sleep)
{
    /* Remember that the sleep sequence has started. */
    sleep->sleeping = true;

    /* Tell the serial terminal what the user needs to do next. */
    printf("Sleep requested. Release SLEEP_PIO, then the racer will sleep.\r\n");
    fflush(stdout);

    /* Wait for the first press to be released before power is turned off. */
    sleep_button_release_wait();
}

/*
   Actually enter WAIT mode.
   The main program has already turned off LEDs and power outputs before this
   is called.  The next press on SLEEP_PIO/WKUP2 should wake the MCU.
*/
void racer_sleep_wait_for_wake(racer_sleep_t *sleep)
{
    printf("WAIT sleep ON using WKUP2. Press SLEEP_PIO again to wake.\r\n");
    fflush(stdout);

    /* This function returns after the WKUP2 wake event. */
    wait_mode_wkup2_sleep();

    /* The MCU has woken, so the module state can go back to awake. */
    sleep->sleeping = false;
}

/*
   Final logging after the main program has restored outputs.
   The parameter is currently unused, but it keeps the function shape similar
   to the other sleep module calls that operate on racer_sleep_t.
*/
void racer_sleep_finish(racer_sleep_t *sleep)
{
    /* Avoid an unused-parameter warning without changing the API. */
    (void)sleep;

    printf("WAIT sleep OFF by SLEEP_PIO/WKUP2.\r\n");
    fflush(stdout);
}
