#ifndef TUNER_BRIDGE_H
#define TUNER_BRIDGE_H

// =============================================================================
//  tuner_bridge.h  -  connects mouse-firmware.ino to the UNCHANGED
//                     motors.cpp / sensors.cpp / API.cpp
// =============================================================================
//
//  Pulled in by ONE line at the end of sensors.h:   #include "tuner_bridge.h"
//  Every .cpp that uses the tuning #defines includes sensors.h, so from here:
//
//   1. LIVE VALUES   Each tuning #define of motors.h / sensors.h (STRAIGHT_PWM,
//                    KP_..., thresholds, ...) is replaced by a field of g_live.
//                    g_live starts from the TUNED VALUES block in
//                    mouse-firmware.ino and the web tuner can change it while
//                    the mouse runs.  The numbers still written in motors.h and
//                    sensors.h are NOT used any more - the .ino block is.
//
//   2. ToF CALIBRATION  Your 23 Sep bench tables (in the .ino) are applied to
//                    every VL53L0X reading inside sensors.cpp, plus a
//                    median-of-3 filter, without editing sensors.cpp.
//
//   3. HOOKS         motors.cpp and API.cpp calls are watched so the tuner can
//                    record a trace, stop a run from the phone, draw the maze
//                    map, and mirror left/right when the maze opens to the LEFT
//                    of the start (MAZE_OPENS).
//
//  Nothing in here holds a tuning value.  Change values in the .ino.
//  Removing the include line from sensors.h returns the firmware to the old
//  hard-coded headers (the .ino then no longer compiles in tuner form).
// =============================================================================

// -----------------------------------------------------------------------------
// Which source file is being compiled?  Decided from the include order of the
// unchanged files, BEFORE this header includes anything itself:
//   sensors.cpp : includes sensors.h first           -> nothing else seen yet
//   motors.cpp  : motors.h, pins.h, then sensors.h   -> MOTORS_H and PINS_H
//   API.cpp     : API.h, motors.h, then sensors.h    -> API_H
//   the .ino    : defines TUNER_IMPL before its includes
// -----------------------------------------------------------------------------
#if defined(TUNER_IMPL)
  #define TB_UNIT_SKETCH 1
#elif defined(API_H)
  #define TB_UNIT_API 1
#elif defined(MOTORS_H) && defined(PINS_H)
  #define TB_UNIT_MOTORS 1
#elif !defined(MOTORS_H) && !defined(PINS_H)
  #define TB_UNIT_SENSORS 1
#endif

#include <Arduino.h>
#include "motors.h"
#include "pins.h"

#if defined(TB_UNIT_SENSORS) || defined(TB_UNIT_SKETCH)
#include <Wire.h>
#include <VL53L0X.h>
#endif

// -----------------------------------------------------------------------------
// 1. LIVE VALUES
// -----------------------------------------------------------------------------
#define TB_TOF_COUNT 5   // index order: 0 front, 1 left-front, 2 left-rear, 3 right-front, 4 right-rear

struct TunerLive
{
    // --- replaces motors.h ---
    int   motorAInverted;
    int   motorBInverted;
    float wheelDiameterMm;
    float encoderCountsPerRev;
    float cellDistanceMm;
    int   straightPwm;
    int   straightMinPwm;
    float kpEncoderSync;
    float kpGyroHeading;
    int   turnPwm;
    int   turnSlowPwm;
    float turnSlowStartDeg;
    float turnStopDegLeft;
    float turnStopDegRight;
    int   turnTimeoutMs;
    float kpSideLateral;
    float kpSideAngle;
    float kpSideDanger;

    // --- replaces sensors.h ---
    float targetSideDistanceMm;
    float sidePairSpacingMm;
    int   sideDangerMm;
    int   frontWallThresholdMm;
    int   sideWallThresholdMm;
    int   sideStrongWallThresholdMm;
    int   frontEmergencyMm;
    int   sideHardStopMm;

    // --- new, handled in the .ino ---
    int   tofUseCalibration;          // 1 = apply the bench tables
    int   tofMedianFilter;            // 1 = median of the last 3 readings
    float tofTrimMm[TB_TOF_COUNT];    // added after calibration
    int   mazeOpens;                  // 0 = RIGHT of the start, 1 = LEFT, 2 = AUTO
    int   mazeSize;                   // 0 = 10x10, 1 = 16x16
    int   goalMode;                   // 0 = centre 2x2, 1 = manual 2x2 (goalForward / goalSideways)
    int   goalForward;                // manual goal: cells forward from the start cell
    int   goalSideways;               // manual goal: cells sideways into the maze
    int   fastContinuous;             // 1 = fast runs drive each straight without stopping per cell
    int   cellStopMs;                 // pause after each cell in the tuner tests
};

extern TunerLive g_live;

// Set by the STOP button.  Makes the move/turn that is running finish at once
// (it brakes and returns) and makes the next API move/turn report a failure.
extern volatile bool g_tbAbort;

// Read a live value fresh every time (it can be changed from the WiFi task).
#define TB_LIVE(field) (*(volatile __typeof__(g_live.field) *)&(g_live.field))

// ---- motors.h ----
#undef  MOTOR_A_INVERTED
#define MOTOR_A_INVERTED              (TB_LIVE(motorAInverted) != 0)
#undef  MOTOR_B_INVERTED
#define MOTOR_B_INVERTED              (TB_LIVE(motorBInverted) != 0)
#undef  WHEEL_DIAMETER_MM
#define WHEEL_DIAMETER_MM             (TB_LIVE(wheelDiameterMm))
#undef  ENCODER_COUNTS_PER_REV
#define ENCODER_COUNTS_PER_REV        (TB_LIVE(encoderCountsPerRev))
#undef  CELL_DISTANCE_MM
#define CELL_DISTANCE_MM              (g_tbAbort ? 0.0f : TB_LIVE(cellDistanceMm))
#undef  STRAIGHT_PWM
#define STRAIGHT_PWM                  (TB_LIVE(straightPwm))
#undef  STRAIGHT_MIN_PWM
#define STRAIGHT_MIN_PWM              (TB_LIVE(straightMinPwm))
#undef  TURN_PWM
#define TURN_PWM                      (TB_LIVE(turnPwm))
#undef  TURN_SLOW_PWM
#define TURN_SLOW_PWM                 (TB_LIVE(turnSlowPwm))
#undef  TURN_SLOW_START_DEG
#define TURN_SLOW_START_DEG           (TB_LIVE(turnSlowStartDeg))
// TURN_STOP_DEG is only used inside turn90(bool leftTurn), so it can pick the
// left or right value from turn90's own parameter.
#undef  TURN_STOP_DEG
#define TURN_STOP_DEG                 (g_tbAbort ? -1.0e6f : ((leftTurn) ? TB_LIVE(turnStopDegLeft) : TB_LIVE(turnStopDegRight)))
#undef  TURN_TIMEOUT_MS
#define TURN_TIMEOUT_MS               ((uint32_t)TB_LIVE(turnTimeoutMs))
#undef  KP_ENCODER_SYNC
#define KP_ENCODER_SYNC               (TB_LIVE(kpEncoderSync))
#undef  KP_GYRO_HEADING
#define KP_GYRO_HEADING               (TB_LIVE(kpGyroHeading))
#undef  KP_SIDE_LATERAL
#define KP_SIDE_LATERAL               (TB_LIVE(kpSideLateral))
#undef  KP_SIDE_ANGLE
#define KP_SIDE_ANGLE                 (TB_LIVE(kpSideAngle))
#undef  KP_SIDE_DANGER
#define KP_SIDE_DANGER                (TB_LIVE(kpSideDanger))

// ---- sensors.h ----
#undef  TARGET_SIDE_DISTANCE_MM
#define TARGET_SIDE_DISTANCE_MM       (TB_LIVE(targetSideDistanceMm))
#undef  SIDE_SENSOR_PAIR_SPACING_MM
#define SIDE_SENSOR_PAIR_SPACING_MM   (TB_LIVE(sidePairSpacingMm))
#undef  FRONT_WALL_THRESHOLD_MM
#define FRONT_WALL_THRESHOLD_MM       (TB_LIVE(frontWallThresholdMm))
#undef  SIDE_WALL_THRESHOLD_MM
#define SIDE_WALL_THRESHOLD_MM        (TB_LIVE(sideWallThresholdMm))
#undef  SIDE_STRONG_WALL_THRESHOLD_MM
#define SIDE_STRONG_WALL_THRESHOLD_MM (TB_LIVE(sideStrongWallThresholdMm))
#undef  FRONT_EMERGENCY_MM
#define FRONT_EMERGENCY_MM            (TB_LIVE(frontEmergencyMm))
#undef  SIDE_DANGER_MM
#define SIDE_DANGER_MM                (TB_LIVE(sideDangerMm))
#undef  SIDE_HARD_STOP_MM
#define SIDE_HARD_STOP_MM             (TB_LIVE(sideHardStopMm))

// -----------------------------------------------------------------------------
// Functions implemented in mouse-firmware.ino
// -----------------------------------------------------------------------------
uint16_t tb_tofProcess(uint8_t address, uint16_t rawMm);   // calibration + filter

float    tb_hookYaw();
void     tb_hookResetYaw();
void     tb_hookSideGuidance(SideGuidance *guidance);
uint16_t tb_hookFront();
void     tb_hookDelay(uint32_t ms);
void     tb_hookAnalogWrite(uint8_t pin, int value);
void     tb_hookDigitalWrite(uint8_t pin, uint8_t level);

bool     tb_hookWallFront();
bool     tb_hookWallLeft();
bool     tb_hookWallRight();
bool     tb_hookMoveOneCell();
bool     tb_hookMoveCells(int cells);
bool     tb_hookTurnLeft90();
bool     tb_hookTurnRight90();

int      tb_mazeSize();      // 10 or 16, used by API_mazeWidth() / API_mazeHeight()
int      tb_goalX();         // goal 2x2 corner nearest the start, floodfill frame
int      tb_goalY();
int      tb_fastContinuous();  // FAST_CONTINUOUS switch, used by API_fastContinuous()

// -----------------------------------------------------------------------------
// 2. ToF CALIBRATION  (sensors.cpp only)
// sensors.cpp declares its five sensors as "static VL53L0X ...".  Here the name
// VL53L0X is pointed at a subclass whose readRangeContinuousMillimeters()
// passes every reading through tb_tofProcess().  The sensor is identified by
// the I2C address sensors.cpp gave it (TOF1_ADDR .. TOF5_ADDR).
// A timeout (65535) and "nothing in range" (>= 8190) pass through untouched, so
// sensors.cpp still sees them exactly as before.
// -----------------------------------------------------------------------------
#if defined(TB_UNIT_SENSORS)
class TbCalibratedVL53L0X : public VL53L0X
{
public:
    uint16_t readRangeContinuousMillimeters()
    {
        return tb_tofProcess(getAddress(), VL53L0X::readRangeContinuousMillimeters());
    }
};
#define VL53L0X TbCalibratedVL53L0X
#endif

// -----------------------------------------------------------------------------
// 3. HOOKS
// motors.cpp: yaw, side guidance, front reading, PWM and the per-loop delay are
// recorded for the trace and the live page.  Each hook calls the real function.
// -----------------------------------------------------------------------------
#if defined(TB_UNIT_MOTORS)
#define sensors_yawDegrees()          tb_hookYaw()
#define sensors_resetYaw()            tb_hookResetYaw()
#define sensors_readSideGuidance(g)   tb_hookSideGuidance(g)
#define sensors_frontDistanceMm()     tb_hookFront()
#define delay(ms)                     tb_hookDelay(ms)
#define analogWrite(pin, value)       tb_hookAnalogWrite((pin), (value))
#define digitalWrite(pin, level)      tb_hookDigitalWrite((pin), (level))
#endif

// API.cpp: what floodfill asks for.  Used for the maze map, the run log, the
// STOP button and MAZE_OPENS (logical left/right swapped when mirrored).
#if defined(TB_UNIT_API)
#define sensors_wallFront()           tb_hookWallFront()
#define sensors_wallLeft()            tb_hookWallLeft()
#define sensors_wallRight()           tb_hookWallRight()
#define motors_moveOneCell()          tb_hookMoveOneCell()
#define motors_moveCells(n)           tb_hookMoveCells(n)
#define motors_turnLeft90()           tb_hookTurnLeft90()
#define motors_turnRight90()          tb_hookTurnRight90()
#endif

#endif // TUNER_BRIDGE_H
