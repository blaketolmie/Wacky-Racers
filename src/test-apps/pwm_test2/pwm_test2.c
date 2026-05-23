#include <stdio.h>

#include "usb_serial.h"
#include "pwm.h"
#include "pio.h"
#include "delay.h"
#include "panic.h"
#include "target.h"

#define LEFT_PWM_PIO    MOTOR_LEFT_PWM_PIO
#define LEFT_DIR_PIO    MOTOR_LEFT_DIR_PIO
#define RIGHT_PWM_PIO   MOTOR_RIGHT_PWM_PIO
#define RIGHT_DIR_PIO   MOTOR_RIGHT_DIR_PIO

#define PWM_FREQ_HZ 500

#define DUTY_MIN 0
#define DUTY_MAX 100

static const pwm_cfg_t left_pwm_cfg =
{
    .pio = LEFT_PWM_PIO,
    .period = PWM_PERIOD_DIVISOR(PWM_FREQ_HZ),
    .duty = PWM_DUTY_DIVISOR(PWM_FREQ_HZ, 0),
    .align = PWM_ALIGN_LEFT,
    .polarity = PWM_POLARITY_HIGH,
    .stop_state = PIO_OUTPUT_LOW
};

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

static void motors_stop(pwm_t left_pwm, pwm_t right_pwm)
{
    /* Stop logic from working code */
    pio_output_high(STAT3_PIO);
    pio_output_low(STAT0_PIO);

    pio_output_high(LEFT_DIR_PIO);
    pio_output_low(RIGHT_DIR_PIO);

    pwm_duty_set(left_pwm, PWM_DUTY_DIVISOR(PWM_FREQ_HZ, 0));
    pwm_duty_set(right_pwm, PWM_DUTY_DIVISOR(PWM_FREQ_HZ, 0));
}

static void motors_set(pwm_t left_pwm, pwm_t right_pwm,
                       int left_command, int right_command)
{
    int left_duty;
    int right_duty;

    left_command = clamp(left_command, -100, 100);
    right_command = clamp(right_command, -100, 100);

    left_duty = abs_int(left_command);
    right_duty = abs_int(right_command);

    if ((left_command == 0) && (right_command == 0))
    {
        motors_stop(left_pwm, right_pwm);
        printf("Stopped\r\n");
        fflush(stdout);
        return;
    }

    /*
       Direction logic matches your working code:

       Forward:
           LEFT_DIR  LOW
           RIGHT_DIR LOW

       Reverse:
           LEFT_DIR  HIGH
           RIGHT_DIR HIGH
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

    pwm_duty_set(left_pwm, PWM_DUTY_DIVISOR(PWM_FREQ_HZ, left_duty));
    pwm_duty_set(right_pwm, PWM_DUTY_DIVISOR(PWM_FREQ_HZ, right_duty));

    printf("Left = %d, Right = %d\r\n", left_command, right_command);
    fflush(stdout);
}

static void prompt_command(void)
{
    printf("\r\nEnter left right duty (-100 to 100): ");
    fflush(stdout);
}

static void process_command(pwm_t left_pwm, pwm_t right_pwm)
{
    char buffer[80];
    int left_command;
    int right_command;

    if (!fgets(buffer, sizeof(buffer), stdin))
        return;

    if (sscanf(buffer, "%d %d", &left_command, &right_command) == 2)
    {
        motors_set(left_pwm, right_pwm, left_command, right_command);
        pio_output_toggle(LED_STATUS_PIO);
    }
    else
    {
        printf("Invalid input. Example: 50 -50\r\n");
        fflush(stdout);
    }

    prompt_command();
}

int main(void)
{
    pwm_t left_pwm;
    pwm_t right_pwm;

    pio_config_set(LED_STATUS_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(STAT0_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(STAT1_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(STAT2_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(STAT3_PIO, PIO_OUTPUT_HIGH);

    pio_config_set(HBRIDGE_ENABLE_PIO, PIO_OUTPUT_HIGH);
    pio_output_high(HBRIDGE_ENABLE_PIO);

    pio_config_set(SLEEP_PIO, PIO_OUTPUT_HIGH);
    pio_output_high(SLEEP_PIO);

    pio_config_set(LEFT_DIR_PIO, PIO_OUTPUT_LOW);
    pio_config_set(RIGHT_DIR_PIO, PIO_OUTPUT_LOW);

    left_pwm = pwm_init(&left_pwm_cfg);
    if (!left_pwm)
        panic(LED_ERROR_PIO, 1);

    right_pwm = pwm_init(&right_pwm_cfg);
    if (!right_pwm)
        panic(LED_ERROR_PIO, 2);

    pwm_channels_start(pwm_channel_mask(left_pwm) | pwm_channel_mask(right_pwm));

    usb_serial_stdio_init();

    motors_stop(left_pwm, right_pwm);

    printf("\r\nWacky Racer USB PWM motor control ready.\r\n");
    printf("Input format: left right\r\n");
    printf("Duty range: -100 to 100\r\n");
    printf("Examples:\r\n");
    printf("  50 50    = forward straight\r\n");
    printf(" -50 -50   = reverse straight\r\n");
    printf("  50 25    = forward turning one way\r\n");
    printf("  25 50    = forward turning the other way\r\n");
    printf("  50 -50   = spin one way\r\n");
    printf(" -50 50    = spin the other way\r\n");
    printf("  0 0      = stop\r\n");
    fflush(stdout);

    prompt_command();

    while (1)
    {
        process_command(left_pwm, right_pwm);
    }

    return 0;
}