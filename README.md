# RP2350-Zero Doom Port

Porting *Doom* (via the [rp2040-doom](https://github.com/kilograham/rp2040-doom) engine)
to a Waveshare **RP2350-Zero** board driving an ST7789 SPI TFT, with a custom
display backend replacing the stock VGA output.

## Hardware

- **Waveshare RP2350-Zero** — RP2350A, 520 KB SRAM, no PSRAM, 4 MB flash. Main target board.
- **ST7789 240x320 SPI TFT** — display, wired to SPI0.
- **74HC165 shift register + 8 buttons** — Game Boy-style gamepad (Up/Down/Left/Right/Select/Start/B/A), read over 3 wires.
- **MAX98357A I2S amp** — audio output (SFX + FM-synth music).
- **2-wire coin vibration motor + 2N2222 NPN transistor** — haptic feedback (buzz on firing a shot, longer buzz on taking damage).
- **Li-ion/LiPo battery + TP4056 (charging) + MT3608 (boost to 5V)** — portable power. MT3608 output feeds the board's `VBUS` header pin (the onboard `ME6217C33M5G` LDO regulates that down to 3.3V for logic); a resistor divider off the raw battery line feeds an ADC pin for an on-screen battery level indicator.
- **ESP32-S3 SuperMini** — used as a USB↔UART bridge for reading RP2350 debug logs (RP2350 has no free USB while running Doom's UART console).

### ST7789 wiring (SPI0)

| ST7789 pin | RP2350-Zero pin |
|---|---|
| GND | GND |
| VCC | 3V3 (OUT) |
| SCL | GP2 (SCK) |
| SDA | GP3 (MOSI) |
| DC  | GP4 |
| RES | GP5 |
| CS  | GP6 |
| BL/BLK | GP7 |

### UART debug bridge wiring

| RP2350-Zero pin | ESP32-S3 SuperMini pin |
|---|---|
| GP8 (UART1 TX) | GPIO 2 |
| GND | GND |

Both boards run 3.3V logic — direct connection, no level shifter needed.

### Gamepad wiring (74HC165)

| 74HC165 pin | RP2350-Zero pin |
|---|---|
| SH/LD | GP10 |
| CLK | GP11 |
| QH (data out) | GP12 |
| VCC | 3V3 |
| GND | GND |

### Audio wiring (MAX98357A, I2S)

| MAX98357A pin | RP2350-Zero pin |
|---|---|
| DIN | GP26 |
| BCLK | GP27 |
| LRC | GP28 |
| VIN | 5V |
| GND | GND |
| SD, GAIN | not connected (board defaults) |

### Vibration motor wiring (haptic feedback)

Bare coin motor draws more current than a GPIO can source directly, so it's
switched through an NPN transistor:

| Component leg | Connects to |
|---|---|
| GP13 | 1kΩ resistor → transistor base |
| Base | also 10kΩ pull-down to GND (keeps motor off during MCU boot/reset) |
| Collector | motor (–) lead |
| Emitter | GND |
| Motor (+) lead | 3V3 |
| Flyback diode (1N4148/1N4001) | across the motor, cathode to 3V3 — protects the transistor from the motor's back-EMF |

Transistor is a **2N2222** (NPN, TO-92) — check the datasheet for your specific
part's physical pin order (E/B/C order varies by manufacturer).

### Battery voltage sensing wiring

| Component leg | Connects to |
|---|---|
| Raw battery (+) line (before the MT3608 boost, e.g. TP4056 `BAT+`) | Resistor divider top |
| Divider midpoint | GP29 (ADC3) |
| Divider bottom | GND |

GP29/ADC3 was confirmed free of any onboard use by checking the official
Waveshare schematic — unlike the original Raspberry Pi Pico, this board does
*not* hardwire GP29 to a VSYS-sense divider, so it's safe to repurpose. Exact
resistor values aren't critical: the firmware (`pico/battery.c`) calibrates
off a real multimeter measurement (battery voltage vs. voltage read back at
the ADC pin) rather than assuming nominal resistor ratios — see the
`CAL_BATTERY_MV`/`CAL_PIN_MV` constants if you rebuild the divider with
different resistors and need to recalibrate.

## Repository layout

- **`RPi2350zero.ino`** — Arduino bring-up sketch: verifies the ST7789 wiring/init
  using the Adafruit ST7789/GFX libraries before any performance-critical code is written.
- **`rp2040-doom/`** — fork of kilograham's `rp2040-doom` (CMake/Pico SDK project,
  not Arduino), with a custom bare-metal ST7789 SPI display backend for RP2350.
  See `rp2040-doom/src/pico/st7789.c`, `i_video.c` (`USE_ST7789_SPI`),
  `pico/gamepad.c` (button input, text entry, volume control),
  `pico/haptic.c`/`.h` (vibration motor feedback on fire/damage), and
  `pico/battery.c`/`.h` (battery level sensing + the on-screen indicator
  drawn into the top letterbox bar).
- **`uart_bridge/`** — Arduino sketch for the ESP32-S3, forwarding the RP2350's
  UART1 debug output to a USB serial port for logging/debugging.
- **`rpi2350zero.png`** — board/wiring reference photo.

## Build (rp2040-doom, RP2350 target)

Requires: Pico SDK 2.2.0+, pico-extras, arm-none-eabi-gcc, CMake, Ninja, and a
host C compiler (e.g. MinGW) to build pioasm/picotool from source. Run all of
this from the `rp2040-doom/` directory in a **cmd.exe** (Command Prompt) window
— PATH/env vars set with `set` only last for that one window, so re-run them
in every fresh prompt.

```cmd
set PATH=<arm-gcc-bin>;%PATH%
set PICO_TOOLCHAIN_PATH=<arm-gcc-bin>

cmake -G Ninja -S . -B build-rp2350 -DCMAKE_BUILD_TYPE=MinSizeRel -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2_vga -DPICO_BOARD_HEADER_DIRS="<repo>/rp2040-doom/boards" -DPICO_SDK_PATH="<path-to-pico-sdk>" -DPICO_EXTRAS_PATH="<path-to-pico-extras>"

ninja doom_tiny
```

What each command does:

1. `set PATH=<arm-gcc-bin>;%PATH%` — puts `arm-none-eabi-gcc` (the cross
   compiler that targets the RP2350's Cortex-M33 core) at the front of PATH so
   CMake/Ninja can find it. Windows doesn't persist this outside the current
   window, so it has to be re-run every time you open a new prompt.
2. `set PICO_TOOLCHAIN_PATH=<arm-gcc-bin>` — separately from PATH, the Pico
   SDK's own CMake scripts read this variable to locate the compiler/objcopy/
   etc. toolchain directory.
3. `cmake -G Ninja -S . -B build-rp2350 ...` — configures the build once
   (only needs re-running if you change CMake options or add/remove source
   files in `CMakeLists.txt`):
   - `-G Ninja` — use the Ninja generator (faster incremental builds than the
     default Makefiles/MSVC generator).
   - `-S .` — source directory is here (must be run from `rp2040-doom/`,
     which contains the top-level `CMakeLists.txt`).
   - `-B build-rp2350` — put all generated build files in `build-rp2350/`,
     keeping them out of the source tree.
   - `-DCMAKE_BUILD_TYPE=MinSizeRel` — optimize for smallest binary size, not
     debug info or raw speed — the firmware + WAD have to fit in 4 MB of flash.
   - `-DPICO_PLATFORM=rp2350` — target the RP2350 chip family (not RP2040).
   - `-DPICO_BOARD=pico2_vga` — use the custom board header
     `boards/pico2_vga.h`, which reuses the stock `vgaboard` pin layout but
     targets RP2350 instead of RP2040.
   - `-DPICO_BOARD_HEADER_DIRS="<repo>/rp2040-doom/boards"` — tells the SDK
     where to find that custom board header.
   - `-DPICO_SDK_PATH=...` / `-DPICO_EXTRAS_PATH=...` — paths to your local
     Pico SDK 2.2.0+ and pico-extras checkouts (pico-extras provides the
     scanvideo/I2S libraries this build links against).
4. `ninja doom_tiny` — compiles and links just the `doom_tiny` target (the
   build tree also defines other targets like `doom_tiny_usb` that aren't
   needed here). Output: `build-rp2350\src\doom_tiny.uf2`.

Output: `build-rp2350/src/doom_tiny.uf2` (firmware), paired with the bundled
`doom1.whx` (compressed shareware WAD, doesn't need rebuilding — it's a
prebuilt binary blob checked into the repo).

## Flashing

Firmware and WAD are **two separate loads** to fixed flash addresses. Run from
cmd.exe, with `picotool.exe` on PATH (or give its full path):

```cmd
picotool load build-rp2350\src\doom_tiny.uf2
picotool load -v -t bin doom1.whx -o 0x10040000
picotool reboot
```

What each step does:

1. **Put the board into BOOTSEL mode first**: hold the BOOT button, tap
   RESET, release BOOT. This makes the RP2350 re-enumerate over USB as a
   mass-storage-style device `picotool` can write to, instead of running the
   last-flashed program.
2. `picotool load build-rp2350\src\doom_tiny.uf2` — writes the compiled
   firmware (engine + your gamepad/haptic/display code) into flash. The
   `.uf2` file already embeds its own target address (`0x10000000`), so no
   `-o` offset is needed here.
3. `picotool load -v -t bin doom1.whx -o 0x10040000` — writes the shareware
   WAD as a second, separate flash write at a **fixed** address
   (`0x10040000`), because the engine reads level/asset data straight out of
   flash via XIP (memory-mapped execute-in-place) instead of loading it into
   RAM. `-t bin` tells picotool this file is a raw binary blob (not
   ELF/UF2), which is why an explicit `-o` address is required. `-v` reads
   the flash back afterward and compares it to the file, failing loudly if
   anything didn't write correctly.
4. `picotool reboot` — exits BOOTSEL mode and starts running the
   newly-flashed firmware, equivalent to a power-cycle but done over USB
   without touching the board.

**Iteration tip:** once the WAD has been loaded once, it persists in flash
across reboots and reflashes — only reload it if `doom1.whx` itself changes.
Day-to-day firmware iteration is just step 2 + step 4 (put back in BOOTSEL,
`picotool load ...uf2`, `picotool reboot`) — no need to repeat step 3.

**Verifying a flash without any peripherals attached:** `picotool load`
already verifies the firmware by default, but to double check either write
without touching hardware, put the board in BOOTSEL and run
`picotool verify build-rp2350\src\doom_tiny.uf2` and
`picotool verify -t bin doom1.whx -o 0x10040000` — both must print `OK`. A
board whose WAD was never loaded will show the write region as all `0xff`
(erased flash) instead of matching the file.

## Controls

| Button | Action |
|---|---|
| Up/Down/Left/Right | Move / turn |
| A | Fire; also confirms menu selections |
| B | Use (open doors, switches) |
| Select (hold) | Strafe + run modifier — hold while moving |
| Select (hold alone, no direction, 0.5s) | Next weapon |
| Select + B | Previous weapon |
| Start | Escape (open/close menu) |
| Start + Select | Toggle automap |
| Start + Up/Down | Adjust sfx + music volume live |

**Save-name entry:** Up/Down cycle a candidate letter/digit with a live preview, A locks it in and advances, B inserts a space, Select finalizes the name, Start cancels. (Weapon-cycle/automap/volume chords above are suppressed while this screen is open.)

**Haptic feedback:** a short buzz on landing an actual shot (skipped on a dry-fire click with no ammo), a longer buzz when the player takes damage.

**Battery indicator:** a small battery icon in the top-left of the screen's
top letterbox bar (the 20px black strip above the 320x200 game viewport,
otherwise unused). Sampled every ~2s and mapped to 5 discrete levels
(critical/low/medium/high/full) rather than a precise percentage — Li-ion
voltage-to-charge is too nonlinear for that to be meaningful from voltage
alone. Color: red (critical/low) → yellow (medium) → green (high/full);
fill width scales with level. Only redraws when the level actually changes.

## Status

- [x] ST7789 SPI bring-up (Arduino test sketch)
- [x] `rp2040-doom` builds clean for RP2350 (`doom_tiny` target)
- [x] Custom ST7789 SPI display backend — stable, demo playback renders correctly
- [x] Input (74HC165 gamepad) — movement, fire/use, strafe/run, automap, live volume, save-name text entry
- [x] Audio — SFX + FM-synth music over I2S (MAX98357A)
- [x] Save/load — flash-backed save slots, multiple saves in a row confirmed working
- [x] Haptic feedback — vibration motor buzzes on landing a shot and on taking damage
- [x] Battery indicator — level icon in the top letterbox bar, calibrated against a real multimeter reading
- [ ] Screen-melt wipe transition (disabled to avoid a core1 deadlock)
- [ ] HUD/statusbar correctness pass
