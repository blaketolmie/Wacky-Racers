#ifndef WACKY_RACERS_TUNING_H
#define WACKY_RACERS_TUNING_H

/*
   Main tuning file for the hat and racer apps.

   Put race-day tuning values here instead of hiding magic numbers in the
   application files.  The hat and racer both include this file, so shared
   radio values must stay safe for both boards.
*/

/* Radio packet identity and anti-replay settings. */
#define LINK_VERSION             1u
#define LINK_HAT_ID              0xA1u
#define LINK_RACER_ID            0xB2u
#define LINK_MSG_CONTROL         0x01u
#define LINK_TX_PERIOD_MS        20u
#define LINK_PACKETS_PER_HOP     10u
#define LINK_FAILSAFE_MS         500u
#define LINK_RESYNC_LOOKAHEAD    60u
#define LINK_RESYNC_DWELL_MS     10u
#define LINK_RESYNC_GRACE_MS     120u
#define RACER_LINK_RESTART_MS    5000u
#define LINK_SECRET              0x5A3C9E27u

/*
   nRF24L01 RF_CH hop table.  Channels 1-10 are left unused.
   These values are spaced 4 MHz apart, then ordered so consecutive hops move
   well away from the previous RF channel.
*/
#define LINK_HOP_TABLE_VALUES \
    11, 43, 75, 27, 59, 15, \
    47, 79, 31, 63, 19, 51, \
    23, 55, 35, 67, 39, 71

/*
   Hat control tuning.

   Smaller scale numbers make the same tilt produce a larger PWM command.
   These values intentionally match the original working hat control feel.
   Racer-side motor tuning below can make the car feel punchier without
   changing the controller's IMU mapping.
*/
#define HAT_CONTROL_X_SCALE      2.6f
#define HAT_CONTROL_Y_SCALE      2.6f
#define HAT_CONTROL_DEADZONE     3.85f
#define HAT_CONTROL_PWM_LIMIT    100

/* Hat low-voltage warning threshold in millivolts. */
#define HAT_LOW_VOLTAGE_THRESHOLD_MV 5000

/*
   Racer motor tuning.

   Gain makes normal commands stronger.  Minimum command helps overcome motor
   deadband so small non-zero commands start moving the car sooner.
*/
#define RACER_MOTOR_PWM_FREQ_HZ      500
#define RACER_DRIVE_GAIN_PERCENT     115
#define RACER_DRIVE_MIN_COMMAND      12
#define RACER_DRIVE_MAX_COMMAND      100

/*
   Racer rollover tuning for the ADXL345.

   The ADXL345 is configured for about 250 counts per g.  These thresholds are
   about +/-0.5 g and include hysteresis so bumps do not flicker the control
   swap on and off.
*/
#define RACER_IMU_UPSIDE_DOWN_Z      -125
#define RACER_IMU_UPRIGHT_Z          125

#endif
