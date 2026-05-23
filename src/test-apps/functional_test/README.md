# Racer Radio PWM Test App

This test app is for the **racer board**. It receives motor commands over the nRF24 radio, drives the left and right motors, prints useful messages over USB serial, and also tests the extra racer features like sleep mode, the bumper, low voltage, the DIP switch radio channel, and the LED tape.

I wrote this README to explain what the program is doing in a simple way, so it is easier to test on the real board without having to read every C file first.

## What This App Does

The racer waits for radio messages from the hat board. Each radio message should contain **two numbers**:

```text
left right
```

Each number is a motor PWM command from `-100` to `100`.

Some examples:

```text
50 50
```

Both motors forward.

```text
-50 -50
```

Both motors reverse.

```text
50 -50
```

Spin one way.

```text
0 0
```

Stop both motors.

The values are sent as text in the radio packet. The radio packet size is still fixed at 32 bytes, but the useful part of the message is just the text, for example `50 25`.

## Files In This App

The code is split into small modules so each part has one job.

`radio_pwm_test1.c` is the main program. It starts everything, runs the main loop, reads radio messages, and calls the other modules.

`racer_motors.c` controls the motor PWM and direction pins. The motor logic was kept the same as the working motor test code.

`radio_link.c` sets up the nRF24 radio, receives motor messages, and sends the `STOP` message back to the hat.

`racer_sleep.c` handles the sleep button and puts the MCU into WAIT mode.

`racer_power.c` turns board power outputs on and off when entering or leaving sleep mode.

`racer_radio_channel.c` reads the DIP switches and chooses the radio channel.

`racer_low_voltage.c` checks the battery monitor pin and turns on the low-voltage LED.

`racer_bumper.c` handles the bumper button. It disables the H-bridge for 5 seconds and asks the radio module to send `STOP`.

`racer_heartbeat.c` blinks an LED so it is obvious the main loop is still running.

`racer_ledtape.c` drives the LED tape. The tape is soft green while the board is awake, turns off in sleep mode, and cycles through green, red, blue, rainbow, blocks, and off when `BUTTON_PIO2` is pushed.

`racer_fpv.c` uses the spare `BUTTON_PIO` button to toggle the FPV output on and off.

## Building The App

From this folder:

```powershell
cd "C:\Users\911BL\OneDrive - University of Canterbury\2026\ENCE461\Wacky Racers\Robot-Board\src\test-apps\functional_test"
$env:BOARD = "racer"
mingw32-make
```

This app is meant to be built for the racer board, so do not use the hat board target for this one.

## Serial Output

The program uses USB serial like a terminal. This is only for checking what is happening.

On startup it prints:

```text
Wacky Racer radio PWM motor control ready.
RX input format: left right
Duty range: -100 to 100
BUMPER_PIO sends STOP and disables the H-bridge for 5 seconds
SLEEP_PIO toggles MCU sleep on/off
BUTTON_PIO toggles FPV on/off
BUTTON_PIO2 cycles LED tape: green, red, blue, rainbow, blocks, off
DIP switches choose radio channel: base 84 plus DIP value
Radio channel: ...
```

When a radio message is received, it prints the received message and the motor values.

Example:

```text
RX: 50 50
Left = 50, Right = 50
```

If the message is not two numbers, it prints an invalid message warning.

## Radio Message Format

The racer expects one radio packet with two numbers inside it:

```text
left right
```

The numbers are separated by a space.

Valid range:

```text
-100 to 100
```

The code uses `sscanf(buffer, "%d %d", &left_command, &right_command)`, so it is expecting two integer values.

The racer also sends a stop message back to the hat when the bumper is pushed:

```text
STOP
```

That message is also put inside a fixed 32-byte nRF24 packet. The important part is the text `STOP` at the start of the packet.

## DIP Switch Radio Channel

The DIP switches choose the radio channel. This means multiple cars can run without all using the same radio channel.

The base channel is:

```text
84
```

The DIP switches add a 4-bit number to this base channel.

The switches are active-low, which means:

```text
switch OFF = input high = 0
switch ON  = input low  = 1
```

DIP switch values:

| Switch | Value |
| --- | ---: |
| DIP 1 | 1 |
| DIP 2 | 2 |
| DIP 3 | 4 |
| DIP 4 | 8 |

Examples:

| DIP Setting | Channel |
| --- | ---: |
| all OFF | 84 |
| DIP 1 ON | 85 |
| DIP 2 ON | 86 |
| DIP 1 and DIP 2 ON | 87 |
| DIP 4 ON | 92 |
| all ON | 99 |

The hat and racer must use the same channel, or the racer will not receive the PWM messages.

## Bumper Behaviour

`BUMPER_PIO` has two jobs.

When the bumper is pushed:

1. The racer stops both motors.
2. The racer sends `STOP` to the hat over the radio.
3. `HBRIDGE_ENABLE_PIO` is set low for 5 seconds.
4. After 5 seconds, `HBRIDGE_ENABLE_PIO` is set high again.

The serial output should show:

```text
Bumper pushed: H-bridge disabled for 5 seconds and STOP requested
Button pushed: sending STOP to hat...
TX: STOP
H-bridge enabled
```

If the radio send fails, it prints:

```text
TX failed
```

## Sleep Mode

Sleep mode is controlled by `SLEEP_PIO`.

Press the sleep button once to enter sleep mode. After that, you can let go of the button and the board should stay asleep. Press the same button again to wake the board.

So the button works like this:

```text
press once  = sleep on
release     = still sleeping
press again = wake up
```

This is done by waiting for the button release before the MCU actually sleeps. That stops the board from instantly waking up just because the same press is still being held.

## What Type Of Sleep Is Used

This app uses SAM4S **WAIT mode**.

`SLEEP_PIO` is on `PA2`, and `PA2` is `WKUP2` on the SAM4S. That means the MCU can use this pin as a wakeup input.

In WAIT mode:

1. The CPU stops running while asleep.
2. The wakeup pin can wake the MCU.
3. After wakeup, the code continues from after the WAIT mode command.
4. It does not restart from the beginning of `main()`.

This is different from BACKUP mode. BACKUP mode saves more power, but waking from BACKUP mode is more like pressing the reset button. The program starts again from the start of `main()`.

For WAIT mode, `racer_sleep.c` enables the SAM4S PMC fast startup input for WKUP2. There is also commented code in `racer_sleep.c` showing how BACKUP mode could be used later if needed.

## What Turns Off In Sleep Mode

When sleep mode starts, the code turns off things that waste power:

| Output | Sleep State |
| --- | --- |
| `HBRIDGE_ENABLE_PIO` | low |
| `FPV_ENABLE_PIO` | low |
| `RADIO_OFF_PIO` | low |
| `IMU_ENABLE_PIO` | low |
| board LEDs | off |
| LED tape | off |

When the board wakes up, these outputs are turned back on and the radio is set up again.

FPV is slightly different because it has its own button toggle. Sleep mode always turns `FPV_ENABLE_PIO` off, but when the board wakes up it restores the FPV state chosen with `BUTTON_PIO`.

`SLEEP_PIO` itself is not driven high or low by the program because it is being used as the pushbutton input.

## FPV Button

`BUTTON_PIO` is the spare button, and this app uses it for FPV.

```text
press BUTTON_PIO once  = FPV off
press BUTTON_PIO again = FPV on
```

The serial output prints:

```text
FPV ON using BUTTON_PIO
FPV OFF using BUTTON_PIO
```

The FPV state is remembered while the program is running. If FPV is off before sleep, it should stay off after wake. If FPV is on before sleep, sleep turns it off temporarily, then wake turns it back on.

## LEDs

There are a few different LEDs doing different jobs.

| LED | Meaning |
| --- | --- |
| `LED_STATUS_PIO` | heartbeat LED, shows the program is running |
| `LED_ERROR_PIO` | low-voltage LED |
| LED tape | cycles green/red/blue/rainbow/blocks/off with `BUTTON_PIO2`, off while sleeping |

The heartbeat LED should blink while the board is awake. It should turn off during sleep mode.

The low-voltage LED turns on when `BATTERY_MONITOR_PIO` reads low. This is active-low logic:

```text
BATTERY_MONITOR_PIO low  = low voltage LED on
BATTERY_MONITOR_PIO high = low voltage LED off
```

## LED Tape

The LED tape is driven from `LEDTAPE_PIO`.

In this test app the LED tape has six modes:

```text
green
red
blue
rainbow
blocks
off
```

Each press of `BUTTON_PIO2` moves to the next mode. After `off`, the next press goes back to green.

The blocks mode is meant to look a bit like Tetris on a 1D LED strip:

1. A coloured block starts at the first LED.
2. It drops along the strip one LED at a time.
3. It lands at the end and stays there.
4. More blocks drop until the strip is full.
5. When the strip is full it does a rainbow celebration.
6. After the celebration it clears and starts dropping blocks again.

`BUTTON_PIO2` is used because the current app is already using `BUMPER_PIO` for bumper/STOP and `SLEEP_PIO` for sleep mode.

The number of LEDs comes from `LED_STRIP_NUMBER` in `src\boards\racer\target.h`. It is currently set to `5`.

The code uses the same idea as the `ledtape_test1` and `ledtape_test2` apps:

1. Create an LED buffer with `ledbuffer_init()`.
2. Put colours into the buffer with `ledbuffer_set()`.
3. Send the buffer to the tape with `ledbuffer_write()`.
4. Clear the buffer before sleep so the tape turns off.

If the number of LEDs on the tape is different, change `LED_STRIP_NUMBER` in `src\boards\racer\target.h`.

## Main Loop Order

The main loop runs at 100 Hz using the pacer.

Each loop does roughly this:

1. Blink/update the heartbeat LED.
2. Check the low-voltage input.
3. Keep the LED tape refreshed and check the LED pattern button.
4. Check the FPV button.
5. Check the bumper.
6. Check the sleep button.
7. Read a radio packet.
8. If the radio packet is valid, update the motors.

Keeping this loop simple makes it easier to debug because each module only does one small job.

## Things To Check If It Does Not Work

If the motors do not move:

- Check the serial output to see if `RX:` messages are being received.
- Check that the radio channel printed on the racer matches the hat.
- Check that the received message has two numbers, for example `50 50`.
- Check that the bumper has not just disabled the H-bridge for 5 seconds.
- Try sending `0 0`, then another small value like `25 25`.

If the radio does not work:

- Check the DIP switches on the racer.
- Check that the hat is using the same channel.
- Check the nRF24 wiring and power.
- Check that `RADIO_OFF_PIO` is high while awake.

If sleep mode wakes up straight away:

- Check that the sleep button is on `SLEEP_PIO`.
- Check that `SLEEP_PIO` is really `PA2/WKUP2`.
- Check that the button wiring is active-low.
- Check that the button is released before trying to wake it again.

If the low-voltage LED is backwards:

- The code currently assumes `BATTERY_MONITOR_PIO` low means low voltage.
- The low-voltage LED is `LED_ERROR_PIO`.
- If the hardware gives the opposite signal, the `!` in `racer_low_voltage_update()` is the part that would need to change.

If the LED tape is wrong:

- Check `LEDTAPE_PIO`.
- Check `LED_STRIP_NUMBER` in `src\boards\racer\target.h`.
- Check that `BUTTON_PIO2` is the button you are pressing for LED tape mode changes.
- Keep pressing `BUTTON_PIO2` to cycle through green, red, blue, rainbow, blocks, off, then back to green.
- Check that the tape has power.
- Remember the tape is meant to turn off in sleep mode.

If FPV does not toggle:

- Check that you are pressing `BUTTON_PIO`, not `BUTTON_PIO2`.
- Check `FPV_ENABLE_PIO`.
- Remember sleep mode forces FPV off while sleeping.

## Quick Test Checklist

1. Build and flash the racer app.
2. Open USB serial.
3. Check the startup message and radio channel.
4. Set the hat to the same channel.
5. Send `50 50` from the hat and check the motors move.
6. Send `0 0` and check the motors stop.
7. Push the bumper and check the H-bridge turns off for 5 seconds.
8. Check the hat receives the `STOP` message.
9. Press `BUTTON_PIO2` and check the LED tape changes to red.
10. Keep pressing `BUTTON_PIO2` and check it cycles blue, rainbow, blocks, off, then green.
11. Press `BUTTON_PIO` and check FPV toggles off.
12. Press `BUTTON_PIO` again and check FPV toggles on.
13. Press the sleep button once and check LEDs, LED tape, and FPV turn off.
14. Release the sleep button and check it stays asleep.
15. Press the sleep button again and check it wakes up.
