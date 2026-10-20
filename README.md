# Micromouse Robot — Interactive Tunning and Callbration

<!-- TEAM: replace the bracketed part above once results are official.
     Add a one-line achievement badge here too, e.g.:
     **[X place / Finalist] — SLIIT ROBOFEST 2026, University Category** -->

An autonomous maze-solving robot built for SLIIT ROBOFEST 2026, using a
flood-fill algorithm shared identically between a PC simulator and real
ESP32 hardware.

---

## Overview

<!-- TEAM: 3-5 sentences. Suggested content, already true for this project --
     feel free to edit wording:
     - The robot must fully autonomously map and solve a 16x16 cell maze
       (18cm cells), within a 14.5cm x 14.5cm size limit and 24V power limit.
     - The core design bet: one shared floodfill.cpp implements the entire
       maze-solving algorithm, and is compiled unchanged against two
       different API.cpp implementations -- one that talks to the mms
       simulator over stdin/stdout, one that talks to real sensors and
       motors -- so the exact same logic runs in both places.
     - [Add: what makes your specific robot/approach notable -- e.g. the
       5-sensor lateral centring, the dual optimistic/confirmed distance
       maps, the turn-cost-optimized fast runs, etc.] -->

## Demo

<!-- TEAM: add a photo or short video/gif of the robot solving the maze.
     ![Robot solving the maze](media/demo.gif) -->

## Key Components

<!-- TEAM: these rows are already accurate based on pins.h/motors.h/sensors.h --
     just confirm part numbers/models and fill in anything marked [confirm]. -->

| Component | Role | Why This Part |
|---|---|---|
| ESP32 [confirm exact board] | Microcontroller | [Why this board specifically] |
| 5x VL53L0X | Time-of-flight distance sensors (front, left-front/rear, right-front/rear) | Millimeter-accurate digital readings; front/rear pairs per side enable lateral centring, not just binary wall detection |
| MPU6050 | 6-axis IMU | Gyro yaw feedback to confirm real 90-degree turn completion, not just timed/open-loop turning |
| TB6612FNG | Dual motor driver | [Why this driver -- e.g. footprint, current rating] |
| 2x [motor model] with encoders | Drive + closed-loop distance/turn feedback | [Stall current, gearbox ratio, why chosen] |
| [Battery spec] | Power source | Must stay under the competition's 24V limit and fixed weight tolerance |

## Architecture

- **Algorithm/hardware separation via a swappable API layer.** `floodfill.cpp` contains 100% of the maze-solving logic and only ever calls functions declared in `API.h`. Two separate `API.cpp` files each implement that same interface -- `simulator/API.cpp` talks to the mms simulator over stdin/stdout, `mouse-firmware/API.cpp` talks to real ToF sensors and motors. The algorithm is identical in both builds; only which `API.cpp` gets linked in changes, at compile time.
- **Tri-state wall storage.** Every wall is `UNKNOWN`, `OPEN`, or `WALL` (not just true/false), letting the algorithm distinguish "confirmed open" from "not yet checked" -- unexplored cells are optimistically treated as passable during exploration.
- **Dual distance maps prove optimality.** Flood fill runs twice per step: once assuming unexplored walls are open (`optimistic_distance`, a best-case lower bound) and once trusting only confirmed walls (`confirmed_distance`, a worst-case upper bound). When these two agree at the start cell, the shortest path is mathematically proven, and the search phase can stop early instead of always using every allowed search run.
- **Search-mode vs. fast-mode move selection.** Search mode biases toward unexplored, less-visited cells to map the maze efficiently. Fast mode uses a dynamic-programming pass (`build_fast_turn_cost`) to precompute the minimum-turn path to the goal from every cell and heading, genuinely minimizing total turns across the whole route rather than just the next step.
- [TEAM: add anything else genuinely load-bearing in your design -- e.g. the 5-sensor centring control loop, the autonomous multi-run state machine, any PCB/mechanical decisions.]

## Engineering Process — Real Bugs We Caught

<!-- TEAM: this section is genuinely valuable -- it shows real engineering
     rigor, not just a finished product. Below are real bugs from this
     project's development; add your own hardware-bring-up bugs too
     (sensor miscalibration, wiring issues, PID tuning problems, etc). -->

- **`=` vs `==` in a wall-state check** silently overwrote maze data on every call instead of just reading it -- caught by manually tracing the logic, not a compiler warning.
- **An enum type used before its own definition** in the source file -- a genuine C requirement (types must be fully defined before anything uses them) that isn't always obvious coming from higher-level languages.
- **Off-by-one loop bounds** (`x < width - 1` instead of `x < width`) silently skipped the last row/column of the maze arrays, leaving uninitialized memory that later got read as if it were real wall data.
- **The BFS queue was never reset between repeated flood-fill calls**, causing it to silently write past the end of its array on the second call -- this made the mouse consistently freeze at the same cell every run, a deterministic-looking bug that was actually memory corruption.
- **Goal-cell coordinate math was duplicated in three separate functions** -- correct in all three, but a real maintenance risk if the goal-size rule ever changed and only two of three copies got updated.
- **Maze size was hardcoded to 16x16 in the hardware API layer**, which would have silently broken testing on a smaller practice maze (goal-seeking would have aimed at the wrong logical coordinates entirely) -- caught before it cost real testing time, by tracing through what `API_mazeWidth()` actually returns on hardware.
- [TEAM: add your own -- sensor threshold calibration surprises, motor/turn tuning findings, anything from bring-up day.]

## Repository Structure

```
simulator/        -> PC build: Main.cpp + API.cpp (talks to mms over stdin/stdout)
mouse-firmware/   -> ESP32 build: mouse-firmware.ino + API.cpp (talks to real hardware)
                     floodfill.cpp/.h  -- shared algorithm, identical in both builds
                     motors.cpp/.h, sensors.cpp/.h, pins.h -- hardware-only
media/            -> [TEAM: photos, videos, diagrams]
docs/             -> [TEAM: any writeups, BOM, presentation slides]
```

## Building and Running

### Simulator (mms)
```
cd simulator
g++ API.cpp Main.cpp -o mouse.exe
```
Requires [mms](https://github.com/mackorone/mms) to be installed and configured with this directory as the algorithm's build/run target.

### Hardware (ESP32)
Open `mouse-firmware/mouse-firmware.ino` in the Arduino IDE with the ESP32 board package installed. Requires the Pololu VL53L0X library, Adafruit MPU6050, and Adafruit Unified Sensor.

<!-- TEAM: add any board-specific settings (exact board name, upload speed, etc). -->

## Verification

- [ ] Confirmed clean build against both `API.cpp` variants
- [ ] Bench-tested single move/turn accuracy against measured reference distances/angles
- [ ] [TEAM: fill in your actual test-maze results -- size, timing, number of successful runs]
- [ ] Competition result: [TEAM: fill in]

## Contributors

| Member | Contribution |
|---|---|
| [Name] | [e.g. Algorithm design and implementation (floodfill.cpp), simulator integration] |
| [Name] | [e.g. Hardware/electronics: sensor and motor integration, PCB/wiring] |
| [Name] | [Contribution] |
| [Name] | [Contribution] |
| [Name] | [Contribution] |

## Acknowledgments

<!-- TEAM: thank SLIIT ROBOFEST organizers, mentors, anyone who helped. -->

## Usage and Permissions

<!-- TEAM: pick one and delete the other.

Option A -- open for others to learn from:
This project is shared under the [MIT License](LICENSE). Feel free to use,
modify, and learn from it.

Option B -- reference only, matching the inspiration repo's stance:
Copyright (c) 2026 [Team name]. All rights reserved.
This repository is shared for portfolio and reference purposes only. No
permission is granted to copy, modify, redistribute, or commercially use
any part of this project without prior written permission. -->
