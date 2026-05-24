# Hat App README

This app runs on the hat board.  The hat is the controller.  It reads the IMU,
turns the driver's movement into left and right motor commands, and sends those
commands to the racer over the nRF24L01 radio.

This README is written for a beginner coder.  It explains what every file in
the hat app does, how the pieces fit together, and why the code is shaped this
way.

## Big Picture

The hat does four main things:

1. Reads the accelerometer/IMU.
2. Converts tilt into `left_pwm` and `right_pwm`.
3. Sends a checked radio control packet every 20 ms.
4. Shows feedback with LEDs, a buzzer, and low-voltage status.

The racer receives the packet and decides whether to trust it.  The hat and
racer must agree on the packet format, IDs, secret, and hop table.

## Current Radio Behaviour

The current hat app uses counter-based channel hopping.  It does not send all
packets on one fixed channel.

Important constants are in `hat.c`:

```c
#define LINK_TX_PERIOD_MS        20u
#define LINK_PACKETS_PER_HOP     10u
#define LINK_SECRET              0x5A3C9E27u
```

The hat sends one control packet every 20 ms.  It stays on one radio channel
for 10 packets, then hops to the next channel.

```text
10 packets * 20 ms = 200 ms per channel
```

The current hop table is:

```c
11, 43, 75, 27, 59, 15,
47, 79, 31, 63, 19, 51,
23, 55, 35, 67, 39, 71
```

Channels `1` to `10` are not used.  The channels are spaced 4 MHz apart, then
ordered so each hop moves well away from the previous channel.

The channel is chosen from the packet counter:

```c
hop_index = (counter / LINK_PACKETS_PER_HOP) % number_of_hop_channels;
channel = link_hop_table[hop_index];
```

That means counters `0` to `9` use the first channel, counters `10` to `19` use
the second channel, and so on.

The hat increments its transmit counter after every scheduled packet, even if
the radio send fails.  That is deliberate.  If one channel is bad, the hat keeps
moving through the hop sequence instead of getting stuck forever on the bad
channel.

When the channel changes, the hat prints:

```text
Radio channel: ...
```

## Radio Packet Format

The hat sends this packet to the racer:

```c
typedef struct __attribute__((packed))
{
    uint8_t version;
    uint8_t sender_id;
    uint8_t receiver_id;
    uint8_t msg_type;
    uint32_t counter;
    int8_t left_pwm;
    int8_t right_pwm;
    uint8_t buttons;
    uint8_t flags;
    uint16_t check;
} link_control_packet_t;
```

Why each field exists:

| Field | Why it exists |
| --- | --- |
| `version` | Lets the racer reject packets from an old packet format. |
| `sender_id` | Tells the racer the packet came from the hat. |
| `receiver_id` | Tells the racer the packet is meant for the racer. |
| `msg_type` | Tells the racer this is a control packet. |
| `counter` | Increases every transmit, so old replayed packets are rejected. |
| `left_pwm` | Signed left motor command from `-100` to `100`. |
| `right_pwm` | Signed right motor command from `-100` to `100`. |
| `buttons` | Spare byte for future button data. |
| `flags` | Spare byte for future status bits. |
| `check` | Small keyed packet check calculated by the MCU. |

The packet is compact and stays under the nRF24L01 32-byte payload limit.

The nRF24L01 radio CRC catches radio corruption, but a recorded packet can still
have a valid radio CRC.  The extra `check` field and counter help the racer
reject packets that are old, edited, or not from the matching hat code.

This is not full cryptographic security.  It is a simple project-level safety
layer that stops simple record-and-loop replay from being accepted as fresh
control data.

## Main Loop Summary

The main loop in `hat.c` uses `pacer_wait()` so it runs at `PACER_RATE`, which
is currently 100 Hz in the hat board `target.h`.  One loop tick is about 10 ms.

Each loop does roughly this:

1. Blink the status LED every so often.
2. Check for a radio `STOP` message from the racer.
3. Check the local bumper-test button.
4. Check the sleep button.
5. Every 20 ms, read the IMU and send a control packet.
6. Update the LED strip.
7. Check the low-voltage ADC.

The hat tries to keep the control loop regular.  The radio transmit schedule is
based on ticks, not on whether a previous send succeeded.

## File By File

### `Makefile`

The Makefile tells the ENCE461 build system how to build the hat app.

Important lines:

```make
PERIPHERALS = pit pwm adc
DRIVERS = pacer usb_serial adxl345 panic nrf24 ledtape
SRC = hat.c hat_radio.c control_logic.c bumper_hit.c radio_channel.c low_voltage.c
BOARD = hat
```

`PERIPHERALS` and `DRIVERS` enable the library code needed by this app.  `SRC`
lists every C file that is compiled.  `BOARD = hat` selects the hat board pin
definitions.

### `hat.c`

This is the main app file.  It owns the overall hat behaviour.

It does these jobs:

- defines the radio packet format,
- defines the hop table,
- creates checked control packets,
- sends a packet every 20 ms,
- increments the transmit counter,
- receives `STOP` messages from the racer,
- starts the bumper-hit feedback pattern,
- handles sleep,
- updates the LED strip,
- checks low voltage.

Important helper functions:

| Function | Job |
| --- | --- |
| `link_channel_for_counter()` | Converts a packet counter into a radio channel. |
| `link_set_channel()` | Changes the nRF24 channel only when needed and prints it. |
| `link_make_check()` | Builds the small keyed packet check. |
| `link_check_packet()` | Verifies the packet check before transmit. |
| `link_pwm_to_int8()` | Clamps motor commands into `-100` to `100`. |
| `link_control_send()` | Builds and sends the control packet. |

`tx_counter` is the hat's transmit counter.  The hat increments it after every
scheduled transmit.  That keeps the hop sequence moving, even if one packet is
not acknowledged.

The sleep section sends one final zero-PWM packet before powering down
peripherals.  That gives the racer a clear stop command before the hat sleeps.

### `hat_radio.c` and `hat_radio.h`

These files wrap the low-level nRF24L01 driver for the hat.

`hat_radio_init()`:

- powers up the radio module using `RADIO_OFF_PIO`,
- configures SPI,
- sets the nRF24 address,
- sets the initial radio channel,
- sets the payload size to 32 bytes,
- returns the `nrf24_t *` handle used by `hat.c`.

The radio address is:

```c
#define RADIO_ADDRESS 0x0123456789LL
```

The hat and racer must use the same address.

`hat_radio_read()` reads packets from the racer.  The main thing the hat expects
back from the racer is:

```text
STOP
```

`hat_radio_stop_received()` checks whether a received text packet is exactly
`STOP`.  If it is, `hat.c` starts the bumper-hit LED and buzzer pattern.

`hat_radio_pwm_send()` is an older helper that sends text PWM values like
`"50 20"`.  The current main control path does not use it because `hat.c` now
sends compact checked packets with counters.

### `control_logic.c` and `control_logic.h`

These files read the ADXL345 accelerometer and convert tilt into motor PWM.

`imu_init()`:

- disables JTAG so needed pins can work as normal GPIO/I2C pins,
- powers the IMU on using `IMU_OFF`,
- waits for power to stabilise,
- starts the TWI/I2C bus,
- creates the ADXL345 driver object.

`read_imu(accel)`:

- checks whether the ADXL345 is ready,
- reads acceleration into `accel[0]`, `accel[1]`, and `accel[2]`,
- returns `1` for success and `0` for failure.

`get_pwm(left, right)`:

- reads the IMU,
- scales x and y acceleration,
- clamps each axis to `-100` to `100`,
- applies a small deadzone,
- mixes x and y into left and right motor commands.

The mixing is:

```c
*left  = (int)(y + x);
*right = (int)(y - x);
```

Think of `y` as forward/back and `x` as steering.  Adding steering to one side
and subtracting it from the other makes the car turn.

The deadzone stops tiny sensor noise near the centre from making the motors
creep.

### `bumper_hit.c` and `bumper_hit.h`

These files create the hat's bumper-hit feedback pattern.

When `bumper_hit_start()` is called:

- the bumper pattern becomes active,
- the buzzer PWM starts,
- the LED strip begins flashing red.

`bumper_hit_update(leds)` must be called repeatedly while active.  It:

- steps through a short sad-trombone style jingle,
- flashes all LEDs red,
- stops after 5 seconds.

`bumper_hit_is_active()` lets `hat.c` know whether the pattern is still running.
While it is active, the hat sends zero PWM instead of normal driving commands.

That means a bumper event on either board should make the controller feedback
visible and stop the racer commands for the bumper window.

### `low_voltage.c` and `low_voltage.h`

These files measure the hat battery voltage using the ADC.

`init_low_voltage()`:

- configures the red LED,
- starts the ADC.

`get_battery_voltage()`:

- reads ADC channel 1,
- converts the ADC value to millivolts,
- prints the voltage to USB serial.

The conversion is:

```c
int adc_voltage = (data[0] * 3300 / 4096) * 2;
```

Why multiply by 2?  The battery voltage is likely divided down before reaching
the ADC pin.  The ADC sees about half the real battery voltage, so the code
multiplies by 2 to estimate the original voltage.

`low_power(true)` toggles the red LED as a warning.  `low_power(false)` turns
the warning off.

In `hat.c`, low voltage is checked against:

```c
#define VOLTAGE_THRESHHOLD 5000
```

So the warning starts when the measured battery estimate is at or below about
5000 mV.

### `radio_channel.c` and `radio_channel.h`

These files read the DIP switches and turn them into a fixed radio channel.

They are legacy helpers.  The current main hat app does not use them for the
driving link because the radio now uses counter-based channel hopping.

The old fixed-channel idea was:

```text
channel = 84 + DIP switch value
```

The DIP switches are active-low, so an ON/closed switch reads low.

These files are still compiled and kept because they may be useful for older
tests or future features.

## Hat Sleep Behaviour

The sleep button is `BUTTON_SLEEP_PIO`.

When it is pressed, `hat.c`:

1. Prints `sleep pressed`.
2. Sends one zero-PWM packet to stop the racer.
3. Increments the transmit counter.
4. Stops the bumper pattern.
5. Clears the LED strip.
6. Powers down the radio and IMU.
7. Enters WAIT sleep using `mcu_sleep()`.
8. On wake, powers the radio and IMU back up.
9. Reinitialises the radio and IMU.
10. Restores the hopping channel based on the current counter.

The hat uses WAIT mode, so it continues from where it slept rather than
restarting `main()`.

## Hat LED Behaviour

There are board LEDs and an LED tape.

The status LED toggles as a simple heartbeat.

The normal LED tape pattern has two moving lights:

- a blue light moving one direction,
- a red light moving the other direction.

If the bumper pattern is active, the normal pattern is overridden.  The LEDs
flash red while the buzzer jingle plays.

## Radio STOP Message From Racer

The racer sends the text message `STOP` when its bumper is pressed.

The hat listens for that message every main-loop tick:

```c
if (hat_radio_read(nrf, buffer))
{
    if (hat_radio_stop_received(buffer))
        bumper_hit_start();
}
```

This lets the hat react to a bumper hit that happened on the racer, even though
the normal driving packets go from hat to racer.

## Building

From the hat app folder:

```powershell
cd "C:\Users\911BL\OneDrive - University of Canterbury\2026\ENCE461\Wacky Racers\Wacky-Racers-Git\src\apps\hat"
& "C:\ence461\tool-chain\msys64\usr\bin\bash.exe" --noprofile --norc -c 'export PATH=/c/ence461/tool-chain/msys64/usr/bin:/c/ence461/tool-chain/gcc-arm-none-eabi-9-2019-q4/bin:/c/ence461/tool-chain/OpenOCD-0.10.0/bin:$PATH; make'
```

The Makefile already sets:

```make
BOARD = hat
```

## Useful Serial Output

Startup prints:

```text
Wacky Hat radio PWM sender ready.
Sending checked control packets every 20 ms
Radio hops every 10 packets, about 200 ms
LED_STATUS toggles after STOP is received
```

When the radio channel changes:

```text
Radio channel: 43
```

When a packet is sent:

```text
TX pwm: 40 35
```

When the racer sends `STOP`:

```text
RX: STOP
```

## Troubleshooting

If the racer does not move:

- Check the hat serial output for `TX pwm: ...`.
- If the hat is sending `0 0`, check whether bumper mode is active.
- Check that the IMU is ready and `get_pwm()` is returning non-zero values.
- Check that the racer has the same packet constants and hop table.
- Check that both radios use the same address.

If radio packets fail:

- Check nRF24 wiring and power.
- Watch for `Radio channel: ...` messages on both boards.
- Remember the current app does not use fixed DIP-switch channels.
- Make sure `LINK_SECRET`, IDs, packet format, and hop table match in both apps.

If the IMU does not work:

- Check `ADXL345_ADDRESS` in the hat board `target.h`.
- Check IMU power on `IMU_OFF`.
- Check the TWI/I2C wiring.
- Watch for `Waiting for accelerometer to be ready...` messages.

If low voltage looks wrong:

- Check the ADC channel and voltage divider.
- Check whether the battery divider really divides by 2.
- Check the `VOLTAGE_THRESHHOLD` value in `hat.c`.

## When Editing This App

If you change the radio packet format, hop table, IDs, or secret in `hat.c`,
make the same change in `racer.c`.  The racer rejects packets unless both sides
agree exactly.

If you change how controls are calculated, try to keep `get_pwm()` returning the
same simple interface:

```c
void get_pwm(int *left, int *right);
```

That keeps the radio code separate from the IMU math.

If you change the bumper pattern, remember that the racer LED strip has a
matching red bumper pattern.  Keeping the two patterns similar makes it easier
to see a bumper event during a race.
