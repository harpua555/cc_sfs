# NOTE!!!   This version of the BTT SFS implementation relies on a not-yet-released OC firmware version.  Do not attempt to use/run this build - it will not function without the specific patch! #


# Advanced Filament Sensor for the Elegoo Carbon Centauri

The Carbon Centauri is a great budget printer, but its filament runout sensor is a simple binary switch which cannot detect filament movement. This means the printer will print in mid-air if the filament gets tangled, breaks after the sensor, or fails to feed for any reason. This project aims to mitigate that issue with only two parts and 4 wires.

## Table of Contents

- [Advanced Filament Sensor for the Elegoo Carbon Centauri](#advanced-filament-sensor-for-the-elegoo-carbon-centauri)
  - [Table of Contents](#table-of-contents)
  - [How it works](#how-it-works)
  - [Video](#video)
  - [Parts List (affiliate links)](#parts-list-affiliate-links)
    - [Optional Parts](#optional-parts)
  - [Wiring (with stock runout detection)](#wiring-with-stock-runout-detection)
  - [Alternate Wiring](#alternate-wiring)
  - [Firmware Installation](#firmware-installation)
  - [WebUi](#webui)
  - [Configuring jam detection](#configuring-jam-detection)
  - [3D printed case/adapter](#3d-printed-caseadapter)
  - [Known Issues / Todo](#known-issues--todo)
  - [Updating](#updating)
  - [Building from Source for unsupported boards](#building-from-source-for-unsupported-boards)
  - [Development](#development)
    - [Firmware](#firmware)
    - [Web UI](#web-ui)

## How it works

The Elegoo CC does not have an input for a filament movement sensor like the BTT SFS 2.0, but it does have an open websocket communication layer. Using a cheap off the shelf ESP32 with a Big Tree Tech SFS 2.0 as a replacment, we can track the status of the printer, detect that filament has stopped moving and send a Websocket command to pause the print. This is not as accurate as a direct firmware integration would be, and requires configuring a timeout relative to your print speed, but it can still save prints when configured correctly. The build is relatively simple, requiring only two GPIO pins, 5v, and ground connections.

## Video

[![Tutorial video for building this mod](https://img.youtube.com/vi/6JYKpZ3HX2Y/0.jpg)](https://www.youtube.com/watch?v=6JYKpZ3HX2Y)

## Parts List (affiliate links)

- [ESP32 S3 N8R2](https://amzn.to/4lHsUTo) for their performance, but really, any Esp32 module should work. Also tested with 4mb [ESP32 Wroom modules](https://amzn.to/4kjtDbU).
- [Big Tree Tech SFS 2.0](https://amzn.to/40uZ1wN)

### Optional Parts

- [JST-XH 3-pin connectors](https://amzn.to/4l4m1Ll) OR [PCB mount connectors](https://amzn.to/4ns8Ntx). These are only required if you wish to retain the stock runout functionality by wiring directly to the existing runout connector.
- [Electrocookie](https://amzn.to/4lqguyo) (or other) solderable PCB if you wish to do PCB mount connections rather than direct wiring

## Wiring (with stock runout detection)

_*WARNING:*_ Integrating with the stock wiring may void your Elegoo CC warranty. Do this at your own risk. Your printer may be different, make sure to validate with a multimeter before connecting any wires to the CC.

There are only 4 wires used in this project. All connectors are JST-XH 3 pin connectors, even though only 2 pins are used on each of the two SFS 2.0 connectors.

| Wire              | Color |
| ----------------- | ----- |
| Ground            | Black |
| 5V power          | Red   |
| Runout            | Blue  |
| Movement (motion) | Green |

The Elegoo runout sensor connector has 5v, ground, and runout input
Connect 5v from the elegoo to 5vin on esp32, and 5v on the SFS2.0
Connect ground from the elegoo to ground on SFS2.0 and the ESP32
Connect runout from the elegoo to pin 12 on the ESP32 and the blue wire on the SFS 2.0
Connect the green wire from the SFS 2.0 to pin 13 on the ESP32

![Wiring Diagram](wiring.png)

## Alternate Wiring

If you don't want to connect the device directly to the runout sensor, you may choose to simply disable the runout built-in runout detection and rely entirely on this project to pause the print. In that case, you can power the project from USB and connect the SFS wires to to the ESP32. Red to 5v, green to pin 13, blue to pin 12, and black to ground.

![Wiring Diagram without connecting to Carbon](wiring2.png)

## Firmware Installation

1. Flash the firmware and filesystem, this can be done through the [web tool](https://jonathanrowny.com/cc_sfs/)
2. Once it's flashed, it will create a WiFi network called ElegooXBTTSFS20, connect to it with the password elegooccsfs20
3. Go to http://192.168.4.1 in your browser to load the user interface
4. Enter your wifi ssid, password, elegoo IP address and hit "save settings", the device will restart and connect to your network.
5. Access the web UI at anytime by going to http;//ccxsfs20.local

For local development builds (from this repo) you can also flash everything via a single script:

```powershell
python tools/build_and_flash.py --env esp32-s3-dev
```

This will build the Web UI, sync it into the `data/` filesystem image, and then upload both the firmware and filesystem to the ESP32 using PlatformIO.

## WebUi

The WebUI shows the current status, whether filament has runout or stopped. It can be accessed by IP address or by going to [ccxsfs20.local](http://ccxsfs20.local) if you have mdns enabled on your network.

![ui screenshot](ui.png)

## Configuring jam detection

The current firmware uses **extrusion telemetry from the printer (SDCP)** together with the BTT SFS 2.0 pulses to decide when filament has effectively stopped moving. You tune this behaviour from the Web UI **Settings** tab via:

- **Expected Flow Deficit Threshold (mm)** – how many mm of *requested* filament (from SDCP) are allowed to accumulate without enough total-confirmed movement before the print is considered jammed.
- **Expected Flow Window (ms)** – how long that backlog must persist above the threshold before a jam is declared; this acts purely as a hold time (no time-based decay).
- **Start Print Timeout (ms)** – grace period after a print starts before any pause can be triggered.
- **Behavior when SDCP replies are lost** – what to do if SDCP extrusion data stops arriving while printing:
  - Pause the print when SDCP replies stop.
  - Disable jam detection until SDCP replies return (fail‑open).

Setting the deficit threshold too low may cause false positives on noisy prints; setting it too high may let jams run longer before being caught. The recommended approach is to start slightly conservative (lower threshold, shorter window), test on a simple print, and adjust upward until you no longer see spurious pauses.

In this total-extrusion-only firmware the backlog:

- is calculated directly from the printer’s reported `TotalExtrusion`.
- decreases only when pulses arrive (each pulse subtracts the configured `movement_mm_per_pulse` from the backlog).
- resets to zero after a jam/resume cycle so it can grow linearly with the printer’s cumulative total again.
- only starts accumulating once the first SFS movement pulse is detected, so early telemetry isn’t treated as a deficit.

There is no time-based decay; `Expected Flow Window (ms)` is simply the hold period before a sustained backlog triggers a pause.

## 3D printed case/adapter

The files are available in [models](/models) directory or on [MakerWorld](https://makerworld.com/en/models/1594174-carbon-centauri-x-bigtreetech-sfs-2-0-mod)

## Known Issues / Todo

- [ ] Prints with ironing will fail, as there is no filament movement
- [ ] update from GH rather than using easyota
- [x] use UDP ping to find/update Elegoo CC ip address like octoeverywhere does (exposed as the
      "Auto-detect printer" button in the Web UI settings)
- [ ] maybe integrate with octoeverywhere as an alternative client, so you don't need another rpi or docker container?
- [ ] support more boards like the Seeed Studio XIAO S3
- [ ] printhead cover fall protection

## Updating

Updates are available in the web ui in the update tab or by reflashing via the web tool, however, flashing through the webtool will erase all settings. Updating using the update section of the web ui will not override your settings as long as you use the `firmware-only.bin` from the [releases page](https://github.com/jrowny/cc_sfs/releases).

## Building from Source for unsupported boards

This project uses PlatformIO and building is as easy as adding the VSCode extension and hitting build. You can modify the `platformio.ini` file to support any custom board.

## Development

### Firmware

C++ code is a platformio project in `/src` folder. You can find more info [in their getting started guide](https://platformio.org/platformio-ide).

### Tooling & Local Tests

- **PlatformIO Core:** install with `pip install -U platformio` (or via the VSCode extension) and add
  `pio`/`platformio` to your PATH so firmware builds can be launched from a shell.
- **Native toolchain:** the host-side unit tests (`pio test -e native`) require an MSYS2/MinGW
  toolchain that provides `gcc`/`g++` on PATH (e.g. install MSYS2, then `pacman -S
  mingw-w64-ucrt-x86_64-gcc` and add `<msys-root>\ucrt64\bin` to PATH).
- **Python packages for utilities:** the helper CLI in `tools/` depends on `aiohttp`. Install
  everything with `python -m pip install -r tools/requirements.txt`.
- **Offline flow simulation:** `tools/gcode_flow_sim.py` can replay filament flow from any G-code
  file so you can exercise the firmware without printing.

Once these are in place:

```powershell
# build firmware only
pio run -e esp32-s3-dev

# run native unit tests (requires gcc/g++)
pio test -e native

# query the printer via websocket for extrusion metrics
python tools/elegoo_status_cli.py --ip <printer-ip>

# derive synthetic extrusion samples from G-code
python tools/gcode_flow_sim.py my_test_file.gcode --output json

# replay G-code over a mock websocket (repeats at 1x speed)
python tools/gcode_flow_sim.py my_test_file.gcode --serve --repeat --speed 1.0
```

### Web UI

Web UI code is a [SolidJS](https://www.solidjs.com/) app with [vite](https://vite.dev/) in the `/webui` folder, it comes with a mock server. Just run `npm i && npm run dev` in the web folder.
Use `npm run build` in the `/webui` folder (or `python tools/build_and_flash.py`) to copy code into the `/data` folder, followed by the `Upload Filesystem Image` target in PlatformIO if you are flashing manually.
