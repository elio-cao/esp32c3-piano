# ESP32-C3 8-Key Touch Electronic Piano Firmware

A complete firmware for the ESP32-C3 8-key touch electronic piano, built from
the `ESP32C3电子琴2026-08-28.tel` netlist on a bare ESP32-C3FH4 / ESP32-C3FN4
16-pin DevKitM-style module.

The final merged image (`dist/firmware.bin`) is ready to flash with one
esptool command — see [Flashing](#flashing) below.

---

## Hardware

- **MCU**: ESP32-C3FH4 (or C3FN4), 4 MB flash, 16-pin SMD module.
- **Module pinout (U2, 1-16)**:
  `5V GND 3V3 GPIO4 GPIO3 GPIO10 GPIO20 GPIO21 GPIO0 GPIO1 GPIO2 GPIO3 GPIO4 3V3 GND 5V`.
- **8 touch keys**:
  - TP1..TP6 → GPIO2, GPIO3, GPIO4, GPIO5, GPIO6, GPIO7 (ESP32-C3 native
    touch channels, read via `digitalRead()` with internal pull-up).
  - TP7, TP8 → GPIO1, GPIO0 (TTP223-BA6 active-high direct mode, AHLB=0,
    TOG=0). 1 s warm-up at boot.
- **Audio**: LEDC PWM on GPIO20 (10-bit, 88.2 kHz) → 4.7 kΩ + 100 nF RC LPF
  → PAM8403 amplifier. There is **no GPIO18 on this board**, so the DAC
  path is not used.
- **Status LED**: GPIO8 (active HIGH).
- **Amplifier SD**: GPIO9 (active HIGH enables PAM8403).

> ⚠ **Do NOT install a CR2032 battery.** The netlist's `U1` is a 3 V
> battery on the +5 V node; inserting it would back-power the board.
> Run the board from the USB-C 5 V supply only.

## Layout (the 8 piano keys)

| Key | Touch pad | GPIO | Note   | Frequency (Hz) |
|-----|-----------|------|--------|----------------|
| 1   | TP1       | 2    | C4     | 261.63         |
| 2   | TP2       | 3    | D4     | 293.66         |
| 3   | TP3       | 4    | E4     | 329.63         |
| 4   | TP4       | 5    | F4     | 349.23         |
| 5   | TP5       | 6    | G4     | 392.00         |
| 6   | TP6       | 7    | A4     | 440.00         |
| 7   | TP7 (TTP223) | 1  | B4     | 493.88         |
| 8   | TP8 (TTP223) | 0  | C5     | 523.25         |

Only one key is audible at a time (last-pressed wins); releasing all keys
returns the synth to silence after a 50 ms fade-out.

## Flashing

The merged `dist/firmware.bin` (331 744 bytes) is a single image that
includes the second-stage bootloader, partition table, and the application
binary. Flash it with a single command:

```sh
# Linux / macOS / WSL
python -m esptool --chip esp32c3 -p /dev/ttyUSB0 -b 460800 \
    write_flash 0x0 dist/firmware.bin

# Windows PowerShell
python -m esptool --chip esp32c3 -p COM3 -b 460800 `
    write_flash 0x0 dist\firmware.bin
```

After flashing, press the RST button on the DevKitM. The on-board LED will
blink three times (80 ms on / 80 ms off × 3) to confirm boot, then enter
1 s of TTP223 warm-up. After that, touching any of the 8 pads will play
the corresponding note.

### Erase before first flash (optional)

If the board has been flashed before, it is good practice to do a full
erase first:

```sh
python -m esptool --chip esp32c3 -p COM3 erase_flash
```

### Flash using `flash.bat` (Windows)

`flash.bat COM3` will run the same command with the right port.

## Building from source

The CI workflow at `.github/workflows/build.yml` builds this project on
every push. The exact toolchain:

- `platform = espressif32` (PlatformIO)
- `framework = arduino` (the default `arduino-esp32 3.20017.x` shipped
  with espressif32 7.0.1)
- The workflow applies three local patches to the framework because of
  upstream bugs in 3.20017:
  1. `driver/touch_pad.h` (C3 variant) `#include`s the S3-only
     `driver/touch_sensor.h`, which does not exist in the C3 toolchain.
     Patched: re-route the include to a local stub `touch_sensor.h`.
  2. `cores/esp32/HardwareSerial.cpp::serialEventRun` references the
     `Serial` object which is not declared under
     `ARDUINO_USB_CDC_ON_BOOT=1`. Patched: replace the call with a no-op.
  3. `cores/esp32/main.cpp::app_main` calls `Serial.begin()` for the
     default UART, which doesn't compile in CDC mode either. Patched:
     comment out the call.

If you build locally with PlatformIO, the `extra_scripts = post:merge_bin.py`
mechanism also works (merges the three output binaries into
`dist/firmware.bin`). The CI workflow instead uses `esptool merge_bin`
directly because post-scripts fire after every sub-target and were
prone to silent early-exits.

## Project layout

```
esp32c3_piano/
├── platformio.ini         # espressif32 + arduino, USB-CDC, no DAC
├── merge_bin.py           # optional PIO post-script
├── src/
│   ├── pins.h             # GPIO / audio / LED definitions
│   ├── notes.h            # 8 note frequencies + names
│   ├── audio.h / .cpp     # LEDC PWM + 44.1 kHz timer ISR + sine table
│   ├── keys.h  / .cpp     # 8-key scan with 30 ms debounce
│   └── main.cpp           # setup / loop glue
├── flash.bat              # convenience wrapper for esptool write_flash
├── README.md              # this file
└── dist/
    └── firmware.bin       # the merged image to flash at offset 0x0
```

## Pin reference

| Function         | GPIO | Notes                                  |
|------------------|------|----------------------------------------|
| Audio PWM        | 20   | 10-bit @ 88.2 kHz, RC LPF before amp   |
| Status LED       | 8    | active HIGH, blinks 3× at boot         |
| Amp SD (PAM8403) | 9    | active HIGH enables the amplifier      |
| Key 1 (TP1)      | 2    | digitalRead, INPUT_PULLUP              |
| Key 2 (TP2)      | 3    | digitalRead, INPUT_PULLUP              |
| Key 3 (TP3)      | 4    | digitalRead, INPUT_PULLUP              |
| Key 4 (TP4)      | 5    | digitalRead, INPUT_PULLUP              |
| Key 5 (TP5)      | 6    | digitalRead, INPUT_PULLUP              |
| Key 6 (TP6)      | 7    | digitalRead, INPUT_PULLUP              |
| Key 7 (TP7)      | 1    | TTP223-BA6, 1 s warm-up                |
| Key 8 (TP8)      | 0    | TTP223-BA6, 1 s warm-up                |

## Debug status (session 2026-08-28)

The board was bench-tested end-to-end and three issues were uncovered that
this section documents so a future build can resolve them in one pass.

### 1. Flash configuration must be DIO/40MHz on this board
The original `qio/80m` bootloader header, when flashed onto this bare
ESP32-C3FH4 module, made `second-stage bootloader` trip its own WDT in a
loop (`ets_loader.c 78 ...` reset storm). Switching to
`dio/40m/4MB` lets the chip boot reliably. All `esptool write_flash`
commands in this repo must use:

```
--flash_mode dio --flash_freq 40m --flash_size 4MB
```

### 2. NS4165B amplifier enable polarity is LOW
The on-board amplifier is an NS4165B (eSOP8 AB/D audio amp), NOT a
PAM8403. Per its datasheet:

| SD pin level | behaviour            |
|--------------|----------------------|
| LOW          | **normal operation** |
| HIGH         | shutdown (muted)     |

Therefore `AMP_SD_ACTIVE_LEVEL` in `src/pins.h` must be `LOW`. The
10 kΩ pull-down on R5 keeps SD low at boot so the amp wakes up enabled.
`main.cpp` now drives SD to `LOW` after `keys_init()`.

### 3. GPIO0 (TP8) needs software bypass on this PCB layout
The 16-pin module boots fine on its own. As soon as it is seated on
this PCB the chip stops reaching `setup()` (no blue-LED 3-blink, USB
COM disappears). The root cause is the way the 8 keys are wired to
GPIO2..GPIO7 + GPIO0/GPIO1 strapping pins (the A/B test of pulling
GPIO8 low and disconnecting GPIO0's pull-up did NOT bring the chip
back, so the issue is not a single strapping pin but the whole module
losing its boot flow on this PCB).

Until the PCB is reworked, `kKeyGpio[7]` is set to `-1` (disabled)
and `main.cpp` calls `gpio_reset_pin(0)` + `pinMode(0, INPUT_DISABLE)`
in `setup()` so GPIO0 cannot source/sink into the unknown external
circuit. TP8 is therefore **not responsive**; the other 7 keys still
work normally.

To re-enable TP8, do BOTH of:
1. Set `kKeyGpio[7] = GPIO_NUM_0` in `src/pins.h`.
2. Remove the `gpio_reset_pin(0)` block in `src/main.cpp`.

### Build pipeline state
GitHub Actions runs of the `Build ESP32-C3 piano firmware` workflow
were consistently failing during the `Tool Manager` step
(`espressif/toolchain-riscv32-esp @ 8.4.0+2021r2-patch5` download
timeout) on 2026-08-28 UTC evening. Local PlatformIO reproduction
also stalls on the same toolchain download. The source tree is in a
buildable state — the next run after GitHub's package CDN recovers
should produce `dist/firmware.bin` (≈ 331 kB).

`dist/firmware_piano_v3_backup.bin` is the most recently confirmed
buildable image (NS4165B LOW-active SD, GPIO0 bypassed) — keep it
as a recovery artifact until the next CI build succeeds.

## License

MIT.
