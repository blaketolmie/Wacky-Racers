/*
   Motor driver for the racer board.

   The main program gives this module left and right commands from -100 to
   +100.  This module clamps those values, sets the direction pins, and updates
   the PWM duty cycle that drives the H-bridge inputs.
*/
#include <stdio.h>

#include "racer_motors.h"
#include "pio.h"
#include "target.h"

#define LEFT_PWM_PIO    MOTOR_LEFT_PWM_PIO
#define LEFT_DIR_PIO    MOTOR_LEFT_DIR_PIO
#define RIGHT_PWM_PIO   MOTOR_RIGHT_PWM_PIO
#define RIGHT_DIR_PIO   MOTOR_RIGHT_DIR_PIO

#define PWM_FREQ_HZ 500

#define DUTY_MIN 0
#define DUTY_MAX 100

/* PWM setup for the left motor output pin.  The duty starts at 0 percent. */
static const pwm_cfg_t left_pwm_cfg =
{
    .pio = LEFT_PWM_PIO,
    .period = PWM_PERIOD_DIVISOR(PWM_FREQ_HZ),
    .duty = PWM_DUTY_DIVISOR(PWM_FREQ_HZ, 0),
    .align = PWM_ALIGN_LEFT,
    .polarity = PWM_POLARITY_HIGH,
    .stop_state = PIO_OUTPUT_LOW
};

/* PWM setup for the right motor output pin.  It mirrors the left setup. */
static const pwm_cfg_t right_pwm_cfg =
{
    .pio = RIGHT_PWM_PIO,
    .period = PWM_PERIOD_DIVISOR(PWM_FREQ_HZ),
    .duty = PWM_DUTY_DIVISOR(PWM_FREQ_HZ, 0),
    .align = PWM_ALIGN_LEFT,
    .polarity = PWM_POLARITY_HIGH,
    .stop_state = PIO_OUTPUT_LOW
};

static int clamp(int value, int min, int max)
{
    /* Keep radio commands inside the safe motor command range. */
    if (value > max)
        return max;

    if (value < min)
        return min;

    return value;
}

static int abs_int(int value)
{
    if (value < 0)
        return -value;

    return value;
}

int racer_motors_init(racer_motors_t *motors)
{
    /* Configure status LEDs/outputs and make sure the H-bridge is enabled. */
    pio_config_set(STAT0_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(STAT1_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(STAT2_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(STAT3_PIO, PIO_OUTPUT_HIGH);

    pio_config_set(HBRIDGE_ENABLE_PIO, PIO_OUTPUT_HIGH);
    pio_output_high(HBRIDGE_ENABLE_PIO);

    pio_config_set(LEFT_DIR_PIO, PIO_OUTPUT_LOW);
    pio_config_set(RIGHT_DIR_PIO, PIO_OUTPUT_LOW);

    /* Create the two PWM channels.  A null return means setup failed. */
    motors->left_pwm = pwm_init(&left_pwm_cfg);
    if (!motors->left_pwm)
        return 1;

    motors->right_pwm = pwm_init(&right_pwm_cfg);
    if (!motors->right_pwm)
        return 2;

    pwm_channels_start(pwm_channel_mask(motors->left_pwm)
                       | pwm_channel_mask(motors->right_pwm));

    /* Start from a known safe state: both motors stopped. */
    racer_motors_stop(motors);

    return 0;
}

void racer_motors_stop(racer_motors_t *motors)
{
    /* Turn on the stopped status indication and remove PWM drive. */
    pio_output_high(STAT3_PIO);
    pio_output_low(STAT0_PIO);

    pio_output_high(LEFT_DIR_PIO);
    pio_output_low(RIGHT_DIR_PIO);

    pwm_duty_set(motors->left_pwm, PWM_DUTY_DIVISOR(PWM_FREQ_HZ, 0));
    pwm_duty_set(motors->right_pwm, PWM_DUTY_DIVISOR(PWM_FREQ_HZ, 0));
}

void racer_motors_set(racer_motors_t *motors,
                      int left_command, int right_command)
{
    int left_duty;
    int right_duty;

    left_command = clamp(left_command, -100, 100);
    right_command = clamp(right_command, -100, 100);

    /*
       The sign chooses direction.  The absolute value chooses speed.
       For example, -40 means reverse at about 40 percent.
    */
    left_duty = abs_int(left_command);
    right_duty = abs_int(right_command);

    /* A zero/zero command is the same as an explicit stop. */
    if ((left_command == 0) && (right_command == 0))
    {
        racer_motors_stop(motors);
        printf("Stopped\r\n");
        fflush(stdout);
        return;
    }

    /*
       Direction logic

       Forward:
           LEFT_DIR  LOW
           RIGHT_DIR LOW

       Reverse:
           LEFT_DIR  HIGH
           RIGHT_DIR HIGH

       The left and right H-bridge inputs are wired differently, so one side
       uses 100 - duty in forward and the other side uses it in reverse.
    */

    if (left_command >= 0) {
        pio_output_low(LEFT_DIR_PIO);
        left_duty = 100 - left_duty;
    }

    else {
        pio_output_high(LEFT_DIR_PIO);
    }
    if (right_command >= 0) {
        pio_output_low(RIGHT_DIR_PIO);
    }
    else {
        pio_output_high(RIGHT_DIR_PIO);
        right_duty = 100 - right_duty;
    }


    pio_output_low(STAT3_PIO);
    pio_output_high(STAT0_PIO);

    pwm_duty_set(motors->left_pwm, PWM_DUTY_DIVISOR(PWM_FREQ_HZ, left_duty));
    pwm_duty_set(motors->right_pwm, PWM_DUTY_DIVISOR(PWM_FREQ_HZ, right_duty));

    printf("Left = %d, Right = %d\r\n", left_command, right_command);
    fflush(stdout);
}
