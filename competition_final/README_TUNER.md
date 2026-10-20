# Live WiFi tuner for the v4 firmware

The mouse runs its own WiFi network and serves a tuning page with sliders.
Every value is used immediately by the real motion code (motors.cpp,
sensors.cpp, floodfill.cpp), including full maze runs. When the tuning is
done, the page exports one block that you paste into `mouse-firmware.ino`.

## What changed in this folder

| File | Change |
| --- | --- |
| `mouse-firmware.ino` | Tuner build + race build, and the TUNED VALUES block |
| `tuner_bridge.h` | New. Connects the sketch to the unchanged .cpp files |
| `tuner_page.h` | New. The web page (works offline) |
| `sensors.h` | One line added at the end: `#include "tuner_bridge.h"` |
| `pins.h` | ToF XSHUT pins fixed to the wiring: front 26, LF 27, LR 14, RF 25, RR 13 |
| motors.h/.cpp, sensors.cpp, API.h/.cpp, floodfill.h/.cpp, simulator/ | Unchanged |

The numbers still written in motors.h and sensors.h are no longer used. The
TUNED VALUES block at the top of `mouse-firmware.ino` is where the values live.

## Build

- Arduino IDE with the ESP32 core (2.x or 3.x).
- Libraries: **VL53L0X** by Pololu, **Adafruit MPU6050** (the library manager
  also installs Adafruit Unified Sensor and Adafruit BusIO).
- WiFi, WebServer and Preferences come with the ESP32 core.

## Connect

1. Leave `#define TUNER_WIFI 1` and upload.
2. Power on with the mouse still for 2 s (the gyro calibrates).
3. Join the WiFi **MICROMOUSE-TUNER**, password **mouse1234**.
4. Open **http://192.168.4.1** on the phone, tablet or laptop.
   Android: if it warns "no internet", tap "stay connected" or switch off mobile data.

The Serial Monitor (115200) shows the same messages plus a line per maze cell.

## Logging every test to the laptop

With the mouse on the same WiFi as a laptop, `tools/mouse_log.py` records every
test the tuner runs, with no cable:

1. In `mouse-firmware.ino` set `TUNER_HOTSPOT_SSID` / `TUNER_HOTSPOT_PASS` to a
   phone hotspot or the Windows Mobile hotspot. It must be **2.4 GHz**
   (Android: AP band 2.4 GHz; iPhone: Maximize Compatibility on). Upload the
   tuner build.
2. Join the same hotspot with the laptop. It keeps its internet.
3. `python tools/mouse_log.py find` prints the mouse's address: open it on the
   phone (or `http://micromouse.local`) instead of 192.168.4.1.
4. `python tools/mouse_log.py watch` and run tests from the page as usual. Each
   test lands in `logs/session-<time>/`: the result rows, the values changed
   since the previous test, the whole trace (not only the last 5 s), and for
   maze runs the map and the complete cell-by-cell log. `session.md` has one
   line per test.

`python tools/mouse_log.py dump` saves everything once. If the hotspot is not
found within 15 s the mouse makes MICROMOUSE-TUNER as before; then **Export tab
-> Download log** saves the same information as one text file.

## Use

Work through the tabs left to right: Live, Setup, Straight, Turns, Walls, Maze.
Each tab starts with 2-3 steps saying what to run and what to change.

- A slider change is used by the next move (or the running one).
- **STOP** ends the running move at once (it brakes) and ends the test or maze run.
- **Save** keeps the values in the mouse's flash, so they survive a reset.
  Saved values are ignored as soon as the TUNED VALUES block in the .ino changes,
  so values you hard-code always win.
- **Import** (Setup tab) loads what you saved with the test sketch (`mouse-tests`):
  counts per rev, speeds, gains and stop angle.
- The start delay (Export tab) is the pause between pressing a test and the mouse moving.

## Hard-code for the competition

1. Export tab, **Copy**.
2. In `mouse-firmware.ino`, replace everything from `// ---- BEGIN TUNED VALUES ----`
   to `// ---- END TUNED VALUES ----` with the copied block.
3. Set `#define TUNER_WIFI 0` and upload. The Serial Monitor must say "RACE build, WiFi off".

The race build has no radio at all (rules 2.2: RF modules must be off) and runs
the original program: calibrate, search, return to start, fast runs.
Reprogramming is not allowed after the inspection, so flash the race build before it.

## MAZE_OPENS

floodfill.cpp assumes the maze opens to the RIGHT of the start cell. The rules
allow any corner, and a start that opens to the left made the original firmware
fail every test maze. MAZE_OPENS = AUTO (the default) looks at the first cell
of the start column with exactly one side open (the other side is the outer
wall) and, if the maze opens to the left, swaps left and right for floodfill.
RIGHT and LEFT force one side. Tested on 300 random mazes each way: AUTO
completed all of them in both orientations.

## How it works (for whoever edits the code later)

- `tuner_bridge.h` is included at the end of `sensors.h`, which every file that
  uses the tuning `#define`s includes. It `#undef`s each one and points it at a
  field of `g_live`, which the .ino fills from the TUNED VALUES block and the
  web page changes. So motors.cpp and sensors.cpp compile unchanged but read
  live values.
- `TURN_STOP_DEG` becomes separate left and right values (it is only used inside
  `turn90(bool leftTurn)`).
- ToF calibration: inside sensors.cpp the name `VL53L0X` is pointed at a small
  subclass whose reading passes through the 23 Sep bench tables and a median-of-3
  filter (`tb_tofProcess()` in the .ino). Timeouts and "nothing in range" pass through untouched.
- Hooks in motors.cpp (yaw, side guidance, PWM, delay) feed the live page and the
  trace; hooks in API.cpp feed the maze map and implement MAZE_OPENS. Each hook
  calls the real function.
- The web server runs in its own task on core 0 and never touches I2C or the
  motors; tests run in `loop()` on core 1.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Page does not load | Joined MICROMOUSE-TUNER? Using http:// not https://? Android mobile data off? |
| Mouse resets when WiFi starts | Supply dips when the radio starts: charged battery, or tune on USB power at the bench |
| "ToF / MPU6050 not started" | I2C wiring or a sensor. Direction check and encoder move still work |
| Values back to old ones after a reset | Press Save. Saved values are dropped when the TUNED VALUES block changes |
| One ToF reading moves when you cover another | XSHUT pins in pins.h do not match the wiring |
