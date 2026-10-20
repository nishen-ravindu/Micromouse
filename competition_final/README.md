# ROBOFEST ESP32 Micromouse Firmware - 5 ToF Sensor Layout

This project keeps the maze-solving layer separate from the hardware layer:

```text
mouse-firmware/
├── mouse-firmware.ino    setup() + loop()
├── API.h                 simulator-style interface
├── API.cpp               real hardware implementation of that interface
├── floodfill.h/.cpp      optimistic + confirmed flood-fill/state machine
├── motors.h/.cpp         TB6612FNG + encoder + MPU6050 + 5-ToF wall control
├── sensors.h/.cpp        5 x VL53L0X + MPU6050
├── pins.h                ESP32 pin map
└── README.md
```

`floodfill.cpp` still talks only to `API.h`, so the maze-solving algorithm can
also be used in MMS with the simulator's API implementation.

## Mechanical design used by this firmware

These are engineering starting recommendations, not guaranteed final tuning
values:

- Overall mouse width: about **80 mm**.
- Overall mouse length: about **120 mm**.
- Wheel diameter: **32 mm**.
- Wheel centre-to-centre track: **72 mm**.
- Put the wheel axle very close to the geometric centre of the 80 x 120 mm body.
- Keep the battery and other heavy parts low and close to the axle.
- ToF optical-axis height: about **20-25 mm above the floor**.

For a 180 mm cell pitch and 12 mm wall thickness, the nominal open corridor is
about 168 mm. Its half-width is about 84 mm. With a side sensor face near the
outer edge of an 80 mm body (`x ~= +/-40 mm`), the centred sensor-to-wall
reading is approximately:

```text
84 mm - 40 mm = 44 mm
```

That is why the initial `TARGET_SIDE_DISTANCE_MM` is **44 mm**.

## Recommended 5-sensor positions

Use **one front sensor and two sensors on each side**:

```text
                            FRONT

                    TOF1 front-centre
                         (0,+30)
                            ^
                            |
             +-----------------------------+
             |                             |
   TOF2  <---|  (-40,+25)       (+40,+25) |--->  TOF4
 left-front  |                             |      right-front
             |          WHEEL AXLE         |
             |             (0,0)           |
   TOF3  <---|  (-40,-25)       (+40,-25) |--->  TOF5
 left-rear   |                             |      right-rear
             +-----------------------------+

                  80 mm wide x 120 mm long
```

The coordinates above describe the desired optical-face locations relative to
the axle. Real sensor PCBs have thickness and mounting holes, so get the **sensor
face** as close as practical to those locations rather than forcing the PCB
centre to the exact coordinate.

The front sensor points straight ahead. All four side sensors point exactly 90
degrees sideways. The front/rear sensors on each side are about **50 mm apart**.

This spacing lets the controller measure two different things:

1. **Lateral position**: average front/rear distance tells how close the body is
   to each wall.
2. **Wall angle**: the difference between front and rear readings tells whether
   the mouse is pointing toward or away from the wall.

This is substantially better for centring than one sensor per side.

## Sensor assignment and addresses

- `TOF1` / XSHUT GPIO13 / `0x30` = FRONT-CENTRE
- `TOF2` / XSHUT GPIO14 / `0x31` = LEFT-FRONT
- `TOF3` / XSHUT GPIO27 / `0x32` = LEFT-REAR
- `TOF4` / XSHUT GPIO26 / `0x33` = RIGHT-FRONT
- `TOF5` / XSHUT GPIO25 / `0x34` = RIGHT-REAR
- `TOF6` / GPIO33 stays shut down and is spare.

All VL53L0Xs and the MPU6050 share SDA GPIO21 and SCL GPIO22 at 400 kHz.

## How the 5 sensors are used

### Maze wall detection

`API_wallFront()` uses TOF1.

`API_wallLeft()` fuses TOF2 + TOF3.

`API_wallRight()` fuses TOF4 + TOF5.

The flood-fill algorithm therefore still sees the exact same three logical
questions it saw in the simulator: front wall, left wall, right wall.

### Straight-line centring

During every physical 180 mm movement, `motors.cpp` repeatedly reads all four
side sensors and calculates:

- lateral centring error,
- heading/angle error relative to the side wall(s),
- minimum left/right clearance.

Those values are combined with:

- encoder left/right synchronization,
- MPU6050 yaw correction,
- side-wall centring,
- side-wall angle correction.

If both walls exist, the controller tries to keep the mouse equidistant from
them. If only one wall exists, it maintains about 44 mm from that wall. If
neither wall exists, it falls back to encoder + gyro straight-line control.

There is also an anti-graze safety bias below `SIDE_DANGER_MM`, and a final hard
stop below `SIDE_HARD_STOP_MM`. These are safety guardrails, not a substitute
for tuning.

## Starting thresholds / control values

In `sensors.h`:

```cpp
TARGET_SIDE_DISTANCE_MM       44.0
FRONT_WALL_THRESHOLD_MM       90
SIDE_WALL_THRESHOLD_MM        95
SIDE_STRONG_WALL_THRESHOLD_MM 65
FRONT_EMERGENCY_MM            24
SIDE_DANGER_MM                26
SIDE_HARD_STOP_MM             12
```

These are **starting values only**. The finished mechanical robot must be
measured and calibrated before competition.

## Arduino libraries

Install:

- **VL53L0X by Pololu**
- **Adafruit MPU6050**
- **Adafruit Unified Sensor**

Use an ESP32 Arduino core that supports `analogWrite()`.

## Critical calibration values

### 1. Motor direction

Starter assumption:

```cpp
Motor A = left
Motor B = right
MOTOR_A_INVERTED = 0
MOTOR_B_INVERTED = 1
```

Test with the wheels off the ground first.

### 2. Encoder direction

Forward movement must produce consistent corrected encoder direction on both
wheels. Change `ENCODER_A_INVERTED` / `ENCODER_B_INVERTED` if required.

### 3. Encoder counts per wheel revolution

`ENCODER_COUNTS_PER_REV = 660` is only a placeholder. Measure the actual count
for your exact motor, gearbox and encoder.

### 4. Loaded wheel diameter

The code starts at 32 mm. Measure the real rolling diameter under robot weight
and tune until one commanded cell physically moves centre-to-centre by 180 mm.

### 5. 90-degree turns

Tune the gyro stopping angle and turn PWM after the finished robot is assembled.
The current controller starts braking around a measured 88 degrees to allow for
coast/overshoot.

### 6. Five-ToF calibration

Place the completed mouse perfectly centred and parallel in a straight corridor.
Record all five readings.

The four side readings should be close to each other. Adjust the physical mounts
before using software offsets if possible.

Then deliberately move the robot about 5 mm left/right and rotate it a few
degrees each way. Confirm:

- moving toward the left wall produces a correction to the right,
- moving toward the right wall produces a correction to the left,
- pointing toward the left wall produces a correction to the right,
- pointing toward the right wall produces a correction to the left.

Only after signs are correct should you increase the side-control gains.

## Search / fast-route behaviour

The flood-fill state machine reserves the competition attempts as:

```text
Search run 1
Search run 2 (only if shortest route not yet proven)
Search run 3 (only if still needed)
Fast route run 1
Fast route run 2
```

If `optimistic == confirmed` earlier, search mode ends early and the remaining
attempts can be used for fast-route runs.

## First physical test order

1. Power ESP32 + sensors only. Print/verify all five VL53L0X readings.
2. Verify TOF2/TOF3 really correspond to the left side and TOF4/TOF5 to right.
3. Put the robot centred between two walls and check readings are near the
   expected ~44 mm target.
4. Wheels off the ground: verify motor and encoder directions.
5. Measure encoder counts per revolution.
6. Tune one slow 180 mm straight move with **no walls** using encoder + gyro.
7. Tune one slow 90-degree turn.
8. Test a straight corridor with two side walls and tune lateral centring.
9. Introduce a slight starting angle and tune front/rear side-pair angle control.
10. Test one-wall corridors and openings.
11. Only then run the full flood-fill maze.

No set of initial gains can guarantee zero contact before the real chassis,
motors, wheel traction and sensor offsets are measured. Keep the first tests
slow and increase speed only after repeatable centring is demonstrated.
