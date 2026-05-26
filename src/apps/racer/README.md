# Racer App README

This app runs on the racer board.  It receives driving commands from the hat
over the nRF24L01 radio, checks that the packet is valid, drives the motors,
and keeps the racer safe if radio control is lost.

This README is written for a beginner coder.  It explains both what the code
does and why it is written that way.

## Big Picture

The racer is the car.  It does not decide how fast to drive by itself.  The hat
reads the driver's movement and sends two motor commands:

```text
left_pwm  right_pwm
```

Each value is a signed number from `-100` to `100`.

```text
 100  100   both motors forward
-100 -100   both motors reverse
  60  -60   spin one way
   0    0   stop
```

The racer only applies those values after the radio packet passes several
checks:

1. The packet is from the hat.
2. The packet is meant for the racer.
3. The packet has the expected message type.
4. The packet has the expected packet check value.
5. The packet counter is newer than the last accepted counter.

The counter is important because it rejects simple replay attacks.  If another
radio records one valid packet and plays it again later, the racer should reject
it because the counter is old.

The channel hop is also based on the counter.  That means the hat and racer
move through the same channel table at the same time, without needing to send a
separate "next channel" message.

## Current Radio Behaviour

The current racer app uses counter-based channel hopping, not a single fixed
DIP-switch channel.

Important constants are in `racer.c`:

```c
#define LINK_TX_PERIOD_MS        20u
#define LINK_PACKETS_PER_HOP     10u
#define LINK_FAILSAFE_MS         500u
#define LINK_RESYNC_LOOKAHEAD    60u
#define LINK_RESYNC_DWELL_MS     10u
#define LINK_RESYNC_GRACE_MS     120u
```

What those mean:

| Constant | Meaning |
| --- | --- |
| `LINK_TX_PERIOD_MS` | The hat sends one control packet every 20 ms. |
| `LINK_PACKETS_PER_HOP` | The radio stays on each channel for 10 packets. |
| `LINK_FAILSAFE_MS` | If no valid packet arrives for 500 ms, the motors stop. |
| `LINK_RESYNC_LOOKAHEAD` | When out of sync, scan up to 60 future counters. |
| `LINK_RESYNC_DWELL_MS` | Wait about 10 ms on a scan channel before trying the next one. |
| `LINK_RESYNC_GRACE_MS` | Allow a short quiet time before beginning resync scanning. |

With the current values, the channel changes every:

```text
10 packets * 20 ms = 200 ms
```

The current hop table is:

```c
11, 43, 75, 27, 59, 15,
47, 79, 31, 63, 19, 51,
23, 55, 35, 67, 39, 71
```

Channels `1` to `10` are not used.  The selected channels are spaced 4 MHz
apart, which is friendlier for 2 Mbps nRF24L01 operation than packed adjacent
channels.  The order jumps around the band so consecutive hops are not just
neighbouring RF channels.

There are 18 hop channels.  A full table cycle takes:

```text
18 channels * 10 packets * 20 ms = 3600 ms
```

So the racer visits the same channel again about every 3.6 seconds.

## Radio Packet Format

The racer receives this compact packet from the hat:

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
| `version` | Lets the code reject packets from an old or different packet format. |
| `sender_id` | Must be `LINK_HAT_ID`, so random packets are ignored. |
| `receiver_id` | Must be `LINK_RACER_ID`, so packets for another device are ignored. |
| `msg_type` | Must be `LINK_MSG_CONTROL`, so only control packets reach the motors. |
| `counter` | Increases every hat transmit.  Old counters are treated as replayed packets. |
| `left_pwm` | Signed left motor command from `-100` to `100`. |
| `right_pwm` | Signed right motor command from `-100` to `100`. |
| `buttons` | Spare byte for future button data.  Currently not used. |
| `flags` | Spare byte for future status bits.  Currently not used. |
| `check` | A small keyed packet check calculated from the other packet bytes. |

The packet is well under the nRF24L01 32-byte payload limit.

The nRF24L01 already has a radio CRC, but that CRC only says the radio bytes
were received correctly.  A recorded packet can still have a valid radio CRC.
The MCU-level `check` field gives the racer another way to reject packets that
do not match the expected keyed packet contents.

This is not full cryptographic security.  It is a simple protection layer for a
student race robot.  It helps reject accidental garbage, edited packets, and
simple record-and-loop replay.

## Counter Overflow

The counter is a `uint32_t`, so eventually it goes:

```text
4294967294
4294967295
0
1
2
```

The racer handles that using this idea in `link_counter_is_newer()`:

```c
return (int32_t)(new_counter - last_counter) > 0;
```

Because unsigned subtraction wraps around, `0 - 0xffffffff` becomes `1`.  That
makes counter `0` look newer than counter `0xffffffff`, which is what we want
after overflow.

This works as long as the accepted packet is not more than about half the
32-bit counter range ahead or behind.  At 20 ms per packet, that half-range is
far longer than a race, so it is safe for this project.

## Main Loop Summary

The main loop in `racer.c` runs at 100 Hz.  That means it runs once every 10 ms.

Each loop does roughly this:

1. Wait for the next 10 ms tick using the pacer.
2. Update the heartbeat LED.
3. Check the low-voltage input.
4. Check the FPV button.
5. Update racer IMU orientation and print readings slowly, if the IMU started correctly.
6. Check the bumper.
7. Update the LED tape.
8. Check the sleep button.
9. Poll the radio link.
10. If a valid packet arrives, apply the motor command.
11. If no valid packet has arrived for too long, stop the motors.

The important design idea is that each small module owns one job.  `racer.c`
acts like the conductor.  It decides when each module gets called, but the
details live in smaller files.

## File By File

### `Makefile`

The Makefile tells the ENCE461 build system how to build this app.

Important lines:

```make
PERIPHERALS = pwm pit
DRIVERS = usb_serial pacer nrf24 button panic ledtape adxl345
SRC = racer.c racer_motors.c radio_link.c racer_sleep.c \
      racer_radio_channel.c racer_low_voltage.c racer_heartbeat.c \
      racer_bumper.c racer_ledtape.c racer_power.c racer_fpv.c \
      racer_imu.c
```

`PERIPHERALS` and `DRIVERS` enable library code used by the app.  `SRC` lists
every C file that gets compiled into the racer program.

### `racer.c`

This is the main app file.  It owns the overall behaviour.

It does these jobs:

- defines the radio packet format,
- defines the hop table,
- validates radio packets,
- keeps track of the last valid packet counter,
- scans/resyncs if packets are missed,
- stops the motors on failsafe,
- starts all modules,
- runs the main loop.

The radio helper functions in this file are `static`, which means they are only
used inside `racer.c`.  That keeps the anti-replay logic close to the main radio
receive code and avoids adding a large new module.

Important helper functions:

| Function | Job |
| --- | --- |
| `link_channel_for_counter()` | Converts a packet counter into a radio channel. |
| `link_make_check()` | Builds the small keyed packet check. |
| `link_check_packet()` | Compares the packet's check field with the expected value. |
| `link_counter_is_newer()` | Rejects old counters and handles counter wraparound. |
| `link_packet_header_ok()` | Checks version, sender ID, receiver ID, and message type. |
| `link_packet_valid()` | Runs all packet validity checks. |
| `link_listen_on_channel()` | Sets the nRF24 channel only when it changes, then listens. |
| `link_poll()` | Normal receive plus resync scanning. |
| `process_radio_command()` | Sends valid PWM commands to the motor module. |

The racer only updates `last_valid_counter` after a packet passes all checks.
That is important.  If bad packets could move the counter forward, an attacker
or noisy radio packet could desync the racer.

When the channel changes, the code prints:

```text
Radio channel: ...
```

That is useful when watching USB serial during testing.

### `radio_link.c` and `radio_link.h`

These files are a small wrapper around the low-level nRF24L01 driver.

`radio_link_init(channel)`:

- powers up the radio module using `RADIO_OFF_PIO`,
- configures the SPI bus,
- sets the nRF24 address,
- sets the initial radio channel,
- sets the payload size to 32 bytes,
- returns the `nrf24_t *` handle used by the rest of the program.

The radio address is:

```c
#define RADIO_ADDRESS 0x0123456789LL
```

The hat and racer must use the same address or they will not hear each other.

`radio_link_stop_send()` sends the text message:

```text
STOP
```

That is used when the bumper is pressed.  The racer stops itself, then sends
`STOP` back to the hat so the hat can run its bumper-hit LED and buzzer pattern.

`radio_link_read()` is still present for older text-style packets and debugging,
but the current main radio control path in `racer.c` reads the compact checked
packet directly with `nrf24_read()`.

### `racer_motors.c` and `racer_motors.h`

These files control the H-bridge motor outputs.

The public type is:

```c
typedef struct
{
    pwm_t left_pwm;
    pwm_t right_pwm;
} racer_motors_t;
```

That struct remembers the PWM channels after they are created.

Main functions:

| Function | Job |
| --- | --- |
| `racer_motors_init()` | Configures motor pins, starts PWM channels, and stops motors. |
| `racer_motors_stop()` | Sets both motor PWM outputs to zero. |
| `racer_motors_set()` | Applies signed left/right motor commands. |

The input range is `-100` to `100`.  The code clamps values to that range so a
bad value cannot ask for more than full power.

The sign chooses direction:

```text
positive = forward
negative = reverse
zero     = stopped
```

The absolute value chooses speed.  For example, `-40` means reverse at about 40
percent.

One detail that can look strange to a beginner is this:

```c
left_duty = 100 - left_duty;
```

and a similar correction on the right motor in reverse.  This is because the
left and right H-bridge inputs are wired differently.  The code compensates so
the higher-level command still feels simple: positive means forward, negative
means reverse.

### `racer_bumper.c` and `racer_bumper.h`

These files handle the physical bumper input.

When the bumper is pressed:

1. `racer_bumper_update()` returns `true` for one main-loop tick.
2. `racer.c` stops the motors.
3. `racer.c` sends `STOP` to the hat.
4. The bumper module pulls `HBRIDGE_ENABLE_PIO` low for 5 seconds.
5. The LED tape module is told that the bumper is active.

The 5-second timer is stored as loop ticks:

```c
#define BUMPER_DISABLE_SECONDS 5
#define BUTTON_POLL_RATE 100
#define HBRIDGE_OFF_TICKS (BUMPER_DISABLE_SECONDS * BUTTON_POLL_RATE)
```

Because the main loop runs at 100 Hz, 500 ticks is about 5 seconds.

`racer_bumper_is_active()` stays true for the whole 5-second window.  The LED
tape uses that to flash red while the bumper safety stop is active.

### `racer_ledtape.c` and `racer_ledtape.h`

These files drive the LED strip.

The mode enum is:

```c
typedef enum
{
    RACER_LEDTAPE_MODE_STANDARD,
    RACER_LEDTAPE_MODE_RAINBOW,
    RACER_LEDTAPE_MODE_BLOCKS,
    RACER_LEDTAPE_MODE_OFF,
    RACER_LEDTAPE_MODE_NUM
} racer_ledtape_mode_t;
```

The first/default mode is `standard`.

Modes:

| Mode | What it does |
| --- | --- |
| `standard` | One LED slowly moves back and forth.  It changes colour at each end. |
| `rainbow` | A moving rainbow flows along the strip. |
| `blocks` | A block animation fills the strip, then celebrates with rainbow. |
| `off` | Clears the strip. |

`BUTTON_PIO2` cycles through those modes.

The bumper pattern overrides the normal mode.  When the bumper is active, the
strip flashes red using the same style as the hat.  This makes a bumper hit
visible on both boards.

The code uses a `ledbuffer_t`.  The pattern functions first draw colours into
the buffer, then `ledbuffer_write()` sends the buffer to the real LED strip.
This is easier than trying to drive the strip one LED at a time.

### `racer_sleep.c` and `racer_sleep.h`

These files handle sleep mode.

The racer uses SAM4S WAIT mode.  WAIT mode stops the CPU to save power, but
keeps RAM and program variables alive.  After wakeup, the program continues
from where it went to sleep.

The sleep button is:

```c
#define SLEEP_BUTTON_PIO SLEEP_PIO
```

On the racer board, `SLEEP_PIO` is `PA2`, which is also SAM4S wakeup pin
`WKUP2`.

The sleep sequence is split across several functions:

| Function | Job |
| --- | --- |
| `racer_sleep_init()` | Creates the debounced sleep button object. |
| `racer_sleep_poll()` | Updates the button state each main-loop tick. |
| `racer_sleep_toggle_requested_p()` | True when the button is newly pressed. |
| `racer_sleep_arm()` | Waits for the button to be released before sleep. |
| `racer_sleep_wait_for_wake()` | Enters WAIT mode and returns after wakeup. |
| `racer_sleep_finish()` | Prints the wakeup message. |

The release wait matters.  Without it, the MCU could enter sleep while the
button is still held down, then wake immediately from the same press.

The file also contains a commented BACKUP-mode example.  BACKUP mode saves more
power, but it behaves more like a reset when it wakes.  WAIT mode is simpler for
this app because variables survive sleep.

### `racer_power.c` and `racer_power.h`

These files own board-level power enable pins.

`racer_power_init()` configures the outputs and applies the normal awake state.

`racer_power_sleep_enter()` turns off power-hungry outputs before sleep:

- H-bridge,
- FPV output,
- radio,
- IMU,
- board LEDs.

`racer_power_sleep_exit()` turns the important outputs back on after wake:

- H-bridge,
- radio,
- IMU.

FPV is restored separately by `racer_fpv_apply()` because FPV has its own user
toggle state.

Keeping this code in one file is helpful because sleep mode should not need to
know the details of every power pin.

### `racer_low_voltage.c` and `racer_low_voltage.h`

These files watch for low voltage or bad power.

The racer board uses digital monitor signals rather than the hat's ADC voltage
measurement.

Important details:

- `BATTERY_MONITOR_PIO` is active-low.
- `PGOOD_PIO`, if defined, is treated as active-high power-good.
- If either signal says power is bad, `LED_ERROR_PIO` turns on.

`BATTERY_MONITOR_PIO` is PB5 on the racer board.  PB5 can default to JTAG/TDO
on the SAM4S, so the code calls:

```c
mcu_jtag_disable();
```

That makes PB5 usable as a normal input.

The code also enables pullups so open-drain or disconnected monitor signals do
not float randomly.

### `racer_heartbeat.c` and `racer_heartbeat.h`

These files blink the status LED so you can tell the main loop is alive.

The main loop calls `racer_heartbeat_update()` at 100 Hz.  The module toggles
the LED every 50 ticks, so the LED changes state about every 0.5 seconds.

This does not control the robot.  It is just a useful debugging sign that the
program is still running.

### `racer_fpv.c` and `racer_fpv.h`

These files control FPV power.

`BUTTON_PIO` toggles FPV on and off.  The state is stored in:

```c
typedef struct
{
    button_t button;
    bool enabled;
} racer_fpv_t;
```

That stored `enabled` value matters for sleep.  Sleep mode temporarily turns
FPV off, but after wakeup the racer can restore the user's chosen FPV state.

### `racer_imu.c` and `racer_imu.h`

These files read the racer ADXL345 accelerometer.  The racer uses the IMU to
notice when the car is upside down, and also prints slow debug readings to USB
serial.

`racer_imu_init()`:

- powers the IMU using `IMU_ENABLE_PIO`,
- waits for the IMU power to settle,
- starts the TWI/I2C bus,
- checks that the ADXL345 answers at `ADXL345_ADDRESS`,
- tries the other legal ADXL345 address if the configured one does not answer,
- returns `0` if the IMU is ready.

The racer can still drive if the IMU does not initialise.  That is deliberate:
the IMU is debug information on the racer, while the radio and motor code are
the important race-control path.

`racer_imu_update()` is called every main-loop tick.  It reads the latest
accelerometer value and updates the stored upright/upside-down state.  When the
Z axis is less than about `-0.5 g`, the racer treats itself as upside down.
When the Z axis is greater than about `+0.5 g`, it treats itself as upright
again.  The gap between those two limits is hysteresis, which stops the controls
from flickering during bumps.

When the racer is upside down, `racer.c` swaps the left and right PWM commands
before passing them to `racer_motors_set()`.  This means a packet that arrived
as `left_pwm, right_pwm` is applied as `right_pwm, left_pwm` after a rollover.

`racer_imu_print_readings()` is called every main-loop tick, but it only prints
about once per second.  Printing every 10 ms would flood USB serial and could
slow down radio receiving and motor control.

After sleep, the racer powers the IMU back on and runs `racer_imu_init()` again
because the ADXL345 loses its setup when power is removed.

### `racer_radio_channel.c` and `racer_radio_channel.h`

These files read the DIP switches and turn them into a fixed radio channel.

They are legacy helpers.  The current main racer app does not use them for the
driving link because the radio now uses counter-based channel hopping.

They are still compiled and kept for older tests or future code that wants to
read the DIP switches directly.

The old fixed-channel idea was:

```text
channel = 84 + DIP switch value
```

The DIP switches are active-low.  That means a closed/ON switch reads low.

## What Happens During A Valid Packet

When a valid packet arrives:

1. `link_poll()` returns `true`.
2. `process_radio_command()` prints the counter and PWM values.
3. `racer_motors_set()` applies the motor commands.
4. `last_valid_counter` becomes the packet counter.
5. `last_valid_packet_ms` becomes the current time.
6. `link_synced` becomes true.
7. The resync scan state resets.
8. The radio moves to the channel expected for the next counter.

The racer only moves its counter forward after a valid packet.  That is why old
or edited packets do not reach the motor logic.

## Failsafe

The failsafe is the rule:

```text
if no valid packet has arrived for 500 ms, stop the motors
```

This prevents the car from continuing to drive forever after the hat is turned
off, blocked, or out of range.

The failsafe looks at valid packets only.  Bad packets do not reset the failsafe
timer.

## Useful Serial Output

Startup prints what the app is configured to do.

When the radio channel changes:

```text
Radio channel: 43
```

When a valid packet arrives:

```text
RX counter 1234: 50 40
Left = 50, Right = 40
```

When a stop command is applied:

```text
RX counter 1235: 0 0
Stopped
```

`Stopped` after lots of `0 0` packets is normal if the hat is sending zero PWM.
It means the radio packets are valid and the motor command is stop.

## Building

From the racer app folder, build for the racer board:

```powershell
cd "C:\Users\911BL\OneDrive - University of Canterbury\2026\ENCE461\Wacky Racers\Wacky-Racers-Git\src\apps\racer"
& "C:\ence461\tool-chain\msys64\usr\bin\bash.exe" --noprofile --norc -c 'export PATH=/c/ence461/tool-chain/msys64/usr/bin:/c/ence461/tool-chain/gcc-arm-none-eabi-9-2019-q4/bin:/c/ence461/tool-chain/OpenOCD-0.10.0/bin:$PATH; make BOARD=racer'
```

## Troubleshooting

If the racer does not move:

- Check whether serial shows `RX counter ...`.
- If it shows valid counters with `0 0`, the hat is commanding stop.
- Check whether the bumper has disabled the H-bridge.
- Check whether failsafe is stopping the motors because valid packets stopped.
- Check that the hat and racer have the same packet constants and hop table.

If the radio does not connect:

- Check nRF24 power and wiring.
- Check that both sides use the same radio address.
- Check that both sides use the same hop table.
- Watch serial for `Radio channel: ...` messages.
- Remember the current app does not use fixed DIP-switch channels.

If the LED strip is wrong:

- Check `LEDTAPE_PIO`.
- Check `LED_STRIP_NUMBER` in the racer board `target.h`.
- Press `BUTTON_PIO2` to cycle modes.
- Press the bumper and check that the red bumper flash overrides normal modes.

If low voltage does not work:

- Check `BATTERY_MONITOR_PIO`.
- Check `PGOOD_PIO` if the board uses it.
- Remember the monitor input is active-low.
- Remember PB5 needs JTAG disabled, which this code does in init.

If the racer IMU does not print readings:

- Check that `ADXL345_ADDRESS` in the racer board `target.h` matches the board.
- Check IMU power on `IMU_ENABLE_PIO`.
- Check the TWI/I2C wiring.
- Look for `Racer IMU disabled, init error ...` on USB serial.

If the upside-down steering is backwards:

- Watch the printed `Racer IMU: ... Z=...` value while the racer is upright and upside down.
- The current code assumes upright gives positive Z and upside down gives negative Z.
- If your mounted IMU is the other way around, swap the signs of `RACER_IMU_UPSIDE_DOWN_Z` and `RACER_IMU_UPRIGHT_Z` in `racer_imu.c`.

## When Editing This App

If you change the radio packet format, IDs, secret, or hop table in `racer.c`,
you must make the same matching change in `hat.c`.  The two boards must agree
exactly, or the racer will reject the hat's packets.

If you change motor behaviour, try to keep the public interface simple:

```c
racer_motors_set(&motors, left_pwm, right_pwm);
```

That lets the radio code stay separate from the motor wiring details.

If you add new racer features, prefer the current pattern:

1. Put one feature in its own small `.c` and `.h` pair.
2. Give it an init function.
3. Give it an update function if it needs to run every tick.
4. Call those functions from `racer.c`.
