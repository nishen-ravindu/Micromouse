/*
 * competition_final.ino  -  ROBOFEST 2026 micromouse (v4) + live WiFi tuner
 *
 * TWO BUILDS, chosen by TUNER_WIFI below:
 *
 *   TUNER_WIFI 1  TUNER.  The mouse makes its own WiFi network. Join it from a
 *                 phone, tablet or laptop and open  http://192.168.4.1
 *                 Every slider is used at once by the real motion code
 *                 (motors.cpp, sensors.cpp, floodfill.cpp), full maze runs too.
 *
 *   TUNER_WIFI 0  RACE.   No radio at all (rules: RF modules must be off).
 *                 Same code, values come only from the TUNED VALUES block
 *                 below. Calibrates the gyro, then waits in the start cell:
 *                   MODE  (GPIO19)  SEARCH <-> FAST (FAST needs a known path)
 *                   SPEED (GPIO33)  straight PWM +10 for that mode; hold 2 s: tuned value
 *                   BOOT  (GPIO0)   hold 5 s: erase the saved maze
 *                   start: hand in front of the front sensor 1 s, take it away
 *                 Each start is one run, start -> goal -> start, then it waits
 *                 again. Place it by hand in the start cell, always facing the
 *                 same way (the opening). Maze, mode and speeds stay in flash
 *                 (NVS "comp") through crashes and power-offs.
 *
 * WHEN THE TUNING IS DONE
 *   Export tab -> Copy -> paste over everything between the BEGIN and END
 *   TUNED VALUES lines below -> set TUNER_WIFI 0 -> upload.
 *   Flash the race build BEFORE the inspection: no reprogramming after it.
 *
 * Files: motors.* / sensors.* / API.* / floodfill.* are unchanged, except one
 * include line at the end of sensors.h (tuner_bridge.h) and the ToF XSHUT pin
 * numbers in pins.h, which did not match the wiring.
 * New: tuner_bridge.h (connects this sketch to the unchanged code) and
 *      tuner_page.h (the web page).
 */

#define TUNER_IMPL          // keep ABOVE the includes: tells tuner_bridge.h this is the sketch

// ============================================================================
//  BUILD
// ============================================================================
#define TUNER_WIFI      0                   // 1 = WiFi tuner, 0 = race build (no radio)
#define TUNER_SSID      "MICROMOUSE-TUNER"
#define TUNER_PASSWORD  "mouse1234"          // 8 characters or more

// Optional: join a phone or laptop hotspot (2.4 GHz) instead, so the laptop on
// the same hotspot keeps its internet and can log every test (tools/mouse_log.py).
// Not found within 15 s, or left "": the mouse makes MICROMOUSE-TUNER as before.
#define TUNER_HOTSPOT_SSID  ""
#define TUNER_HOTSPOT_PASS  ""
#define TUNER_HOSTNAME      "micromouse"     // http://micromouse.local on the hotspot

// ============================================================================
//  TUNED VALUES  -  the numbers the mouse really uses.
//  The Export tab prints this whole block; paste it over the old one.
//  The same-named numbers in motors.h / sensors.h are NOT used any more.
// ============================================================================
// ---- BEGIN TUNED VALUES ----
#define TUNE_MOTOR_A_INVERTED                    0        // 0 / 1
#define TUNE_MOTOR_B_INVERTED                    1        // 0 / 1
#define TUNE_WHEEL_DIAMETER_MM                   34.0f    // mm
#define TUNE_ENCODER_COUNTS_PER_REV              735.0f   // counts
#define TUNE_TOF_USE_CALIBRATION                 1        // 0 / 1
#define TUNE_TOF_MEDIAN_FILTER                   1        // 0 / 1
#define TUNE_TOF_TRIM_F_MM                       -10.0f   // mm
#define TUNE_TOF_TRIM_LF_MM                      -6.0f    // mm
#define TUNE_TOF_TRIM_LR_MM                      -16.0f   // mm
#define TUNE_TOF_TRIM_RF_MM                      -10.5f   // mm
#define TUNE_TOF_TRIM_RR_MM                      -8.0f    // mm
#define TUNE_CELL_DISTANCE_MM                    192.0f   // mm
#define TUNE_CELL_STOP_MS                        10       // ms
#define TUNE_STRAIGHT_PWM                        140      // PWM
#define TUNE_STRAIGHT_MIN_PWM                    45       // PWM
#define TUNE_MAX_STRAIGHT_PWM                    255      // PWM
#define TUNE_KP_ENCODER_SYNC                     0.22f    // per count
#define TUNE_KP_GYRO_HEADING                     0.95f    // per deg
#define TUNE_TURN_PWM                            110      // PWM
#define TUNE_TURN_SLOW_PWM                       38       // PWM
#define TUNE_TURN_SLOW_START_DEG                 64.0f    // deg
#define TUNE_TURN_STOP_DEG_LEFT                  88.0f    // deg
#define TUNE_TURN_STOP_DEG_RIGHT                 88.0f    // deg
#define TUNE_TURN_TIMEOUT_MS                     1250     // ms
#define TUNE_TARGET_SIDE_DISTANCE_MM             50.1f    // mm
#define TUNE_KP_SIDE_LATERAL                     0.72f    // per mm
#define TUNE_KP_SIDE_ANGLE                       1.9f     // per deg
#define TUNE_SIDE_SENSOR_PAIR_SPACING_MM         92.0f    // mm
#define TUNE_KP_SIDE_DANGER                      2.8f     // per mm
#define TUNE_SIDE_DANGER_MM                      20       // mm
#define TUNE_FRONT_WALL_THRESHOLD_MM             150      // mm
#define TUNE_SIDE_WALL_THRESHOLD_MM              90       // mm
#define TUNE_SIDE_STRONG_WALL_THRESHOLD_MM       60       // mm
#define TUNE_FRONT_EMERGENCY_MM                  10       // mm
#define TUNE_SIDE_HARD_STOP_MM                   10       // mm
#define TUNE_MAZE_OPENS                          2        // 0 RIGHT, 1 LEFT, 2 AUTO
#define TUNE_MAZE_SIZE                           1        // 0 10x10, 1 16x16
#define TUNE_GOAL_MODE                           0        // 0 CENTRE, 1 MANUAL
#define TUNE_GOAL_FORWARD                        7        // cells
#define TUNE_GOAL_SIDEWAYS                       7        // cells
#define TUNE_FAST_CONTINUOUS                     1        // 0 / 1
// ---- END TUNED VALUES ----

#include "motors.h"
#include "sensors.h"        // also pulls in tuner_bridge.h
#include "floodfill.h"
#include "pins.h"
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <Preferences.h>    // flash (NVS): the tuner's values, the race build's maze and settings

#if TUNER_WIFI
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "tuner_page.h"
#endif

#define TB_VERSION "v4 tuner 2026-09-24"

// ============================================================================
//  ToF CALIBRATION  (bench data 23 Sep 2026, same tables as the test sketch)
//  Average RAW reading when a flat target stood at the TRUE distance (mm from
//  the sensor face). Raw -> true = straight-line interpolation between the
//  points; beyond 200 mm the best-fit slope continues. Rows must increase.
// ============================================================================
static const float TB_CAL_TRUE_MM[5] = { 20.0f, 50.0f, 100.0f, 150.0f, 200.0f };
static const float TB_CAL_RAW_MM[TB_TOF_COUNT][5] = {
    //  2 cm     5 cm    10 cm    15 cm    20 cm
    { 24.07f,  55.02f, 110.72f, 161.52f, 210.28f },   // front
    { 48.41f,  78.30f, 134.39f, 190.80f, 236.29f },   // left-front
    { 44.84f,  83.56f, 132.28f, 189.19f, 233.63f },   // left-rear
    { 40.97f,  70.14f, 123.81f, 176.11f, 226.14f },   // right-front
    { 77.35f, 101.71f, 163.09f, 205.83f, 260.12f },   // right-rear
};
static const float TB_CAL_EXTRA_GAIN[TB_TOF_COUNT] = { 0.9606f, 0.9408f, 0.9527f, 0.9651f, 0.9753f };

// ============================================================================
//  TYPES  (all types before the first function, for the Arduino IDE)
// ============================================================================
enum { TB_INT = 0, TB_FLOAT = 1, TB_BOOL = 2, TB_ENUM = 3 };

struct TbParam
{
    const char *id;       // shown in the tuner; TUNE_<id> in the block above
    const char *tab;      // setup | straight | turns | walls | maze
    const char *group;    // heading inside the tab
    uint8_t     kind;
    void       *ptr;      // field of g_live
    float       def;      // TUNED VALUES block
    float       lo, hi, step;
    const char *unit;
    const char *help;
};

struct TbTraceSample
{
    uint32_t tMs;
    int32_t  encL, encR;
    int16_t  yawCdeg;       // 0.01 deg
    int16_t  latDmm;        // 0.1 mm
    int16_t  angCdeg;       // 0.01 deg
    int16_t  pwmL, pwmR;
    uint16_t front, lMin, rMin;
    uint8_t  step;
    uint8_t  flags;         // 1 left wall, 2 right wall, 4 braking
};

struct TbStepStats          // what the hooks saw during the move / turn now running
{
    uint32_t t0Ms;
    uint32_t lastYawUs;
    float    yaw;
    float    peakRate;
    float    lat, ang;
    uint16_t front, lMin, rMin;          // last values motors.cpp used
    uint16_t lMinSeen, rMinSeen;         // smallest side clearance of the move
    bool     leftWall, rightWall;
};

struct CompButton           // race build start menu: one push button
{
    uint8_t  pin;
    bool     raw;           // last reading, true = pressed (LOW)
    bool     down;          // debounced
    uint32_t rawSinceMs;
    uint32_t downSinceMs;
    bool     fromIdle;      // pressed while idle: only then does the release act
};

// ============================================================================
//  GLOBALS
// ============================================================================
TunerLive     g_live;                    // declared in tuner_bridge.h
volatile bool g_tbAbort = false;

static TbParam tbParams[] = {
    // ---------------- Setup ----------------
    { "MOTOR_A_INVERTED", "setup", "Motor direction", TB_BOOL, &g_live.motorAInverted, TUNE_MOTOR_A_INVERTED, 0, 1, 1, "",
      "Left motor (A). Flip it if the LEFT wheel turns backwards in the direction check." },
    { "MOTOR_B_INVERTED", "setup", "Motor direction", TB_BOOL, &g_live.motorBInverted, TUNE_MOTOR_B_INVERTED, 0, 1, 1, "",
      "Right motor (B). Flip it if the RIGHT wheel turns backwards in the direction check." },
    { "WHEEL_DIAMETER_MM", "setup", "Distance", TB_FLOAT, &g_live.wheelDiameterMm, TUNE_WHEEL_DIAMETER_MM, 25, 40, 0.1f, "mm",
      "Only mm per count = pi x diameter / counts matters. Leave it and let the ruler calibration fix the counts." },
    { "ENCODER_COUNTS_PER_REV", "setup", "Distance", TB_FLOAT, &g_live.encoderCountsPerRev, TUNE_ENCODER_COUNTS_PER_REV, 100, 3000, 0.5f, "counts",
      "Set by the ruler calibration below. Import brings back what you saved with the test sketch." },
    { "TOF_USE_CALIBRATION", "setup", "ToF sensors", TB_BOOL, &g_live.tofUseCalibration, TUNE_TOF_USE_CALIBRATION, 0, 1, 1, "",
      "1 = apply the 23 Sep bench tables. 0 = raw readings (only to compare)." },
    { "TOF_MEDIAN_FILTER", "setup", "ToF sensors", TB_BOOL, &g_live.tofMedianFilter, TUNE_TOF_MEDIAN_FILTER, 0, 1, 1, "",
      "Median of the last 3 readings of each sensor. Removes single spikes, costs about 20 ms of lag." },
    { "TOF_TRIM_F_MM", "setup", "ToF trims", TB_FLOAT, &g_live.tofTrimMm[0], TUNE_TOF_TRIM_F_MM, -40, 40, 0.5f, "mm",
      "Added to the calibrated front reading. Use only if a capture shows this sensor off." },
    { "TOF_TRIM_LF_MM", "setup", "ToF trims", TB_FLOAT, &g_live.tofTrimMm[1], TUNE_TOF_TRIM_LF_MM, -40, 40, 0.5f, "mm",
      "Added to the calibrated left-front reading." },
    { "TOF_TRIM_LR_MM", "setup", "ToF trims", TB_FLOAT, &g_live.tofTrimMm[2], TUNE_TOF_TRIM_LR_MM, -40, 40, 0.5f, "mm",
      "Added to the calibrated left-rear reading." },
    { "TOF_TRIM_RF_MM", "setup", "ToF trims", TB_FLOAT, &g_live.tofTrimMm[3], TUNE_TOF_TRIM_RF_MM, -40, 40, 0.5f, "mm",
      "Added to the calibrated right-front reading." },
    { "TOF_TRIM_RR_MM", "setup", "ToF trims", TB_FLOAT, &g_live.tofTrimMm[4], TUNE_TOF_TRIM_RR_MM, -40, 40, 0.5f, "mm",
      "Added to the calibrated right-rear reading." },

    // ---------------- Straight ----------------
    { "CELL_DISTANCE_MM", "straight", "Distance per cell", TB_FLOAT, &g_live.cellDistanceMm, TUNE_CELL_DISTANCE_MM, 150, 220, 0.5f, "mm",
      "Encoder distance before braking. Stops past the next cell centre: lower. Short: raise. Redo after every speed change." },
    { "CELL_STOP_MS", "straight", "Distance per cell", TB_INT, &g_live.cellStopMs, TUNE_CELL_STOP_MS, 0, 500, 10, "ms",
      "Pause after each cell in the Straight / Sequence / Follow tests, after the 45 ms brake. Lower = less stop between cells." },
    { "STRAIGHT_PWM", "straight", "Speed", TB_INT, &g_live.straightPwm, TUNE_STRAIGHT_PWM, 40, 255, 1, "PWM",
      "Cruise speed. Keep it until the slow full maze passes, then +10 per step." },
    { "STRAIGHT_MIN_PWM", "straight", "Speed", TB_INT, &g_live.straightMinPwm, TUNE_STRAIGHT_MIN_PWM, 20, 200, 1, "PWM",
      "Floor: the last 48 mm ramp down to it and no wheel goes below it. Lowest that always starts from rest, plus 10." },
    { "MAX_STRAIGHT_PWM", "straight", "Speed", TB_INT, &g_live.maxStraightPwm, TUNE_MAX_STRAIGHT_PWM, 40, 255, 1, "PWM",
      "Race build: the SPEED button adds 10 to STRAIGHT_PWM per press, never above this." },
    { "KP_ENCODER_SYNC", "straight", "Heading hold", TB_FLOAT, &g_live.kpEncoderSync, TUNE_KP_ENCODER_SYNC, 0, 2, 0.01f, "per count",
      "Steer per count of left-right difference. No walls: raise until both wheels' mm agree within 2%. Back off if it twitches." },
    { "KP_GYRO_HEADING", "straight", "Heading hold", TB_FLOAT, &g_live.kpGyroHeading, TUNE_KP_GYRO_HEADING, 0, 10, 0.05f, "per deg",
      "Steer per degree of yaw. No walls: raise until 5 cells end under 3 deg. Halve it if the mouse wobbles." },

    // ---------------- Turns ----------------
    { "TURN_PWM", "turns", "Speed", TB_INT, &g_live.turnPwm, TUNE_TURN_PWM, 30, 255, 1, "PWM",
      "Pivot speed for the first part. Raise only after the stop angles are right. Keep the peak rate under 450 deg/s." },
    { "TURN_SLOW_PWM", "turns", "Speed", TB_INT, &g_live.turnSlowPwm, TUNE_TURN_SLOW_PWM, 20, 200, 1, "PWM",
      "Pivot speed near the end. Lowest value that always finishes the turn gives the most repeatable angle." },
    { "TURN_SLOW_START_DEG", "turns", "Angle", TB_FLOAT, &g_live.turnSlowStartDeg, TUNE_TURN_SLOW_START_DEG, 10, 89, 0.5f, "deg",
      "Yaw where the turn drops to the slow speed. Lower it if the angle varies between tries." },
    { "TURN_STOP_DEG_LEFT", "turns", "Angle", TB_FLOAT, &g_live.turnStopDegLeft, TUNE_TURN_STOP_DEG_LEFT, 60, 100, 0.1f, "deg",
      "Yaw where LEFT turns start braking. 4 x left: average short -> raise, long -> lower." },
    { "TURN_STOP_DEG_RIGHT", "turns", "Angle", TB_FLOAT, &g_live.turnStopDegRight, TUNE_TURN_STOP_DEG_RIGHT, 60, 100, 0.1f, "deg",
      "Yaw where RIGHT turns start braking. 4 x right: average short -> raise, long -> lower." },
    { "TURN_TIMEOUT_MS", "turns", "Safety", TB_INT, &g_live.turnTimeoutMs, TUNE_TURN_TIMEOUT_MS, 500, 6000, 50, "ms",
      "A turn slower than this counts as failed and ends the run. Leave it." },

    // ---------------- Walls ----------------
    { "TARGET_SIDE_DISTANCE_MM", "walls", "Centring", TB_FLOAT, &g_live.targetSideDistanceMm, TUNE_TARGET_SIDE_DISTANCE_MM, 20, 80, 0.5f, "mm",
      "Side reading when centred, used when only one wall exists. Capture centred between two walls and use the side average." },
    { "KP_SIDE_LATERAL", "walls", "Centring", TB_FLOAT, &g_live.kpSideLateral, TUNE_KP_SIDE_LATERAL, 0, 5, 0.02f, "per mm",
      "P term: steer per mm off centre. Raise until a 10 mm offset is gone within 2 cells; lower if it swings." },
    { "KP_SIDE_ANGLE", "walls", "Centring", TB_FLOAT, &g_live.kpSideAngle, TUNE_KP_SIDE_ANGLE, 0, 10, 0.05f, "per deg",
      "Acts as the D term: steer per degree of angle to the wall. If it swings, raise this before lowering the P term." },
    { "SIDE_SENSOR_PAIR_SPACING_MM", "walls", "Centring", TB_FLOAT, &g_live.sidePairSpacingMm, TUNE_SIDE_SENSOR_PAIR_SPACING_MM, 20, 100, 0.5f, "mm",
      "Front-to-rear side sensor spacing, centre to centre. Measure it with a ruler." },
    { "KP_SIDE_DANGER", "walls", "Anti-graze", TB_FLOAT, &g_live.kpSideDanger, TUNE_KP_SIDE_DANGER, 0, 10, 0.1f, "per mm",
      "Extra push away from a wall closer than SIDE_DANGER_MM. Leave it unless the mouse still grazes." },
    { "SIDE_DANGER_MM", "walls", "Anti-graze", TB_INT, &g_live.sideDangerMm, TUNE_SIDE_DANGER_MM, 5, 80, 1, "mm",
      "Clearance where that push starts. About two thirds of the centred reading." },
    { "FRONT_WALL_THRESHOLD_MM", "walls", "Wall detection", TB_INT, &g_live.frontWallThresholdMm, TUNE_FRONT_WALL_THRESHOLD_MM, 40, 250, 1, "mm",
      "Front reading below this = wall ahead. Capture with and without a wall one cell ahead, set it halfway (about 140)." },
    { "SIDE_WALL_THRESHOLD_MM", "walls", "Wall detection", TB_INT, &g_live.sideWallThresholdMm, TUNE_SIDE_WALL_THRESHOLD_MM, 40, 200, 1, "mm",
      "Both side sensors below this = wall. Centred wall about 44, no wall about 220; 100-120 is safe." },
    { "SIDE_STRONG_WALL_THRESHOLD_MM", "walls", "Wall detection", TB_INT, &g_live.sideStrongWallThresholdMm, TUNE_SIDE_STRONG_WALL_THRESHOLD_MM, 20, 150, 1, "mm",
      "One side sensor below this is enough for a wall. Keep it above the centred reading plus 20." },
    { "FRONT_EMERGENCY_MM", "walls", "Safety stops", TB_INT, &g_live.frontEmergencyMm, TUNE_FRONT_EMERGENCY_MM, 0, 80, 1, "mm",
      "Front reading below this during a move ends the run (crash guard). Above the nose-touching reading, below the centred-wall reading. 0 = off." },
    { "SIDE_HARD_STOP_MM", "walls", "Safety stops", TB_INT, &g_live.sideHardStopMm, TUNE_SIDE_HARD_STOP_MM, 0, 60, 1, "mm",
      "Either side below this ends the run. Lower to about 10 if you get false stops. 0 = off." },

    // ---------------- Maze ----------------
    { "MAZE_OPENS", "maze", "Start orientation", TB_ENUM, &g_live.mazeOpens, TUNE_MAZE_OPENS, 0, 2, 1, "0 RIGHT, 1 LEFT, 2 AUTO",
      "Which side of the start cell the maze extends to. AUTO decides at the first cell of the start column with exactly one side open." },
    { "MAZE_SIZE", "maze", "Maze size and goal", TB_ENUM, &g_live.mazeSize, TUNE_MAZE_SIZE, 0, 1, 1, "0 10x10, 1 16x16",
      "Cells per side of the maze. Used at the start of every maze run." },
    { "GOAL_MODE", "maze", "Maze size and goal", TB_ENUM, &g_live.goalMode, TUNE_GOAL_MODE, 0, 1, 1, "0 CENTRE, 1 MANUAL",
      "CENTRE = the middle 2x2 of the maze size above. MANUAL = the 2x2 set by GOAL_FORWARD and GOAL_SIDEWAYS." },
    { "GOAL_FORWARD", "maze", "Maze size and goal", TB_INT, &g_live.goalForward, TUNE_GOAL_FORWARD, 0, 14, 1, "cells",
      "MANUAL only. Cells FORWARD from the start cell (start = 0, along the direction the mouse starts facing) to the goal 2x2 cell nearest the start." },
    { "GOAL_SIDEWAYS", "maze", "Maze size and goal", TB_INT, &g_live.goalSideways, TUNE_GOAL_SIDEWAYS, 0, 14, 1, "cells",
      "MANUAL only. Cells SIDEWAYS into the maze from the start column (start = 0) to the goal 2x2 cell nearest the start." },
    { "FAST_CONTINUOUS", "maze", "Fast runs", TB_BOOL, &g_live.fastContinuous, TUNE_FAST_CONTINUOUS, 0, 1, 1, "",
      "0 = fast runs stop at every cell like the search. 1 = fast runs drive each straight of the known route in one go, stopping only before a turn and at the goal / start." },
};
static const int TB_NPARAMS = (int)(sizeof(tbParams) / sizeof(tbParams[0]));

// ---- ToF ----
static uint16_t          tbTofHist[TB_TOF_COUNT][3];
static uint8_t           tbTofHistN[TB_TOF_COUNT];
static uint8_t           tbTofHistI[TB_TOF_COUNT];
static volatile uint16_t tbTofRaw[TB_TOF_COUNT];
static volatile uint16_t tbTofCal[TB_TOF_COUNT];
static const char *const TB_TOF_NAMES[TB_TOF_COUNT] = { "F", "LF", "LR", "RF", "RR" };

// ---- live telemetry (written by the control loop, read by the web task) ----
static volatile bool     tbSensorsOk = false;
static volatile float    tbTelYaw = 0, tbTelRate = 0, tbTelLat = 0, tbTelAng = 0;
static volatile int      tbTelPwmL = 0, tbTelPwmR = 0;
static volatile uint16_t tbTelFront = 8190, tbTelLMin = 8190, tbTelRMin = 8190;
static volatile uint8_t  tbTelWallF = 0, tbTelWallL = 0, tbTelWallR = 0;
static volatile bool     tbTelBraking = false;
static volatile uint8_t  tbPinLevel[40];

// ---- move / turn statistics and trace ----
static TbStepStats          tbStep;
#define TB_TRACE_MAX 1000   // 1200 overflowed ESP32 static DRAM
static TbTraceSample        tbTrace[TB_TRACE_MAX];
static volatile int         tbTraceHead = 0;
static volatile int         tbTraceCount = 0;
static volatile bool        tbTraceOn = false;
static uint32_t             tbTraceT0 = 0;
static uint32_t             tbTraceLastUs = 0;
static uint8_t              tbTraceStepNo = 0;

// ---- maze tracker (what floodfill sees, in its own logical frame) ----
static const int8_t TB_DX[4] = { 0, 1, 0, -1 };     // N E S W
static const int8_t TB_DY[4] = { 1, 0, -1, 0 };
static int8_t   tbMzX = 0, tbMzY = 0;
static uint8_t  tbMzH = 0;
static int8_t   tbMzMirror = 0;                     // -1 undecided (AUTO), 0 normal, 1 mirrored
static uint8_t  tbMzWalls[16][16];                  // bits 0-3 known N E S W, bits 4-7 wall N E S W
static uint16_t tbMzMoves = 0, tbMzTurns = 0;
static uint32_t tbMzT0 = 0;
static bool     tbMzSensedF = false, tbMzSensedR = false;
static bool     tbMzAfterTurn = false;
#define TB_LOG_N   48
#define TB_LOG_LEN 96
static char     tbLog[TB_LOG_N][TB_LOG_LEN];
static char     tbMzFault[80] = "";
static volatile uint16_t tbLogCount = 0;

// ---- shared state between the web task and the control loop ----
static SemaphoreHandle_t tbLock = NULL;
static char              tbMsg[96] = "starting";
#if TUNER_WIFI
static volatile bool     tbBusy = false;
static char              tbTask[20] = "idle";
static volatile bool     tbDirty = false;
static bool              tbLoadedFromFlash = false;

static volatile bool     tbCmdPending = false;
static char              tbCmdName[16];
static char              tbCmdText[64];
static float             tbCmdNum[4];

#define TB_RES_MAX 6144
static char              tbRes[TB_RES_MAX];           // result being built (control loop)
static size_t            tbResLen = 0;
static bool              tbResFirst = true;
static char              tbResShared[TB_RES_MAX] = "{\"type\":\"none\",\"rows\":[],\"note\":\"\"}";
static volatile uint32_t tbResSeq = 0;

static float             tbLastEncAvgCounts = 0.0f;   // for the ruler calibration
#endif

// ============================================================================
//  SMALL HELPERS  (first function of the file: keep it unconditional)
// ============================================================================
static float tbClampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int16_t tbClamp16(float v)
{
    if (v > 32767.0f) return 32767;
    if (v < -32768.0f) return -32768;
    return (int16_t)lroundf(v);
}

static void tbLockTake()
{
    if (tbLock != NULL) xSemaphoreTake(tbLock, portMAX_DELAY);
}

static void tbLockGive()
{
    if (tbLock != NULL) xSemaphoreGive(tbLock);
}

static void tbStatus(const char *fmt, ...)
{
    char buf[sizeof(tbMsg)];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    tbLockTake();
    memcpy(tbMsg, buf, sizeof(tbMsg));
    tbLockGive();
}

static float tbCountsToMm(long counts)
{
    return ((float)counts / g_live.encoderCountsPerRev) * PI * g_live.wheelDiameterMm;
}

static float tbAvgMm()
{
    long l = labs(motors_leftEncoder());
    long r = labs(motors_rightEncoder());
    return tbCountsToMm((l + r) / 2);
}

// ============================================================================
//  PARAMETERS
// ============================================================================
static float tbSetParam(TbParam &p, float v)
{
    if (isnan(v)) v = p.def;
    v = tbClampf(v, p.lo, p.hi);
    if (p.kind == TB_FLOAT)
    {
        *(volatile float *)p.ptr = v;
        return v;
    }
    int iv = (int)lroundf(v);
    *(volatile int *)p.ptr = iv;
    return (float)iv;
}

static void tbApplyDefaults()
{
    memset(&g_live, 0, sizeof(g_live));
    for (int i = 0; i < TB_NPARAMS; i++) tbSetParam(tbParams[i], tbParams[i].def);
}

#if TUNER_WIFI
static float tbGetParam(const TbParam &p)
{
    if (p.kind == TB_FLOAT) return *(float *)p.ptr;
    return (float)(*(int *)p.ptr);
}

static int tbFindParam(const char *id)
{
    for (int i = 0; i < TB_NPARAMS; i++)
    {
        if (strcmp(tbParams[i].id, id) == 0) return i;
    }
    return -1;
}

// Fingerprint of the TUNED VALUES block: saved values are only reloaded while
// the block is unchanged (so values you hard-code are never overridden).
static uint32_t tbDefaultsSignature()
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < TB_NPARAMS; i++)
    {
        const char *s = tbParams[i].id;
        while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
        uint32_t bits;
        float d = tbParams[i].def;
        memcpy(&bits, &d, sizeof(bits));
        for (int b = 0; b < 4; b++) { h ^= (bits >> (8 * b)) & 0xFF; h *= 16777619u; }
    }
    return h;
}

static void tbFormatValue(char *out, size_t n, const TbParam &p, float v)
{
    if (p.kind != TB_FLOAT)
    {
        snprintf(out, n, "%d", (int)lroundf(v));
        return;
    }
    char tmp[20];
    snprintf(tmp, sizeof(tmp), "%.4f", (double)v);
    // trim trailing zeros but keep one digit after the point
    char *dot = strchr(tmp, '.');
    if (dot != NULL)
    {
        char *end = tmp + strlen(tmp) - 1;
        while (end > dot + 1 && *end == '0') *end-- = '\0';
    }
    snprintf(out, n, "%sf", tmp);
}

// Writes the TUNED VALUES block (same layout as at the top of this file).
static size_t tbBuildExport(char *out, size_t n)
{
    size_t len = 0;
    len += snprintf(out + len, n - len, "// ---- BEGIN TUNED VALUES ----\n");
    for (int i = 0; i < TB_NPARAMS && len < n; i++)
    {
        const TbParam &p = tbParams[i];
        char name[48];
        char val[32];
        char line[128];
        snprintf(name, sizeof(name), "TUNE_%s", p.id);
        tbFormatValue(val, sizeof(val), p, tbGetParam(p));
        const char *unit = p.unit;
        if (p.kind == TB_BOOL) unit = "0 / 1";
        if (unit[0] != '\0')
            snprintf(line, sizeof(line), "#define %-40s %-8s // %s\n", name, val, unit);
        else
            snprintf(line, sizeof(line), "#define %-40s %s\n", name, val);
        len += snprintf(out + len, n - len, "%s", line);
    }
    if (len < n) len += snprintf(out + len, n - len, "// ---- END TUNED VALUES ----\n");
    return len < n ? len : n - 1;
}
#endif // TUNER_WIFI

// ============================================================================
//  ToF CALIBRATION + FILTER  (called from sensors.cpp through tuner_bridge.h)
// ============================================================================
static uint16_t tbMedian3(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) { uint16_t t = b; b = c; c = t; }
    if (a > b) { uint16_t t = a; a = b; b = t; }
    return b;
}

static float tbCalibrate(int id, float raw)
{
    const float *r = TB_CAL_RAW_MM[id];
    const float *t = TB_CAL_TRUE_MM;
    const int last = 4;
    float mm;
    if (raw <= r[0])
    {
        mm = t[0] + (raw - r[0]) * (t[1] - t[0]) / (r[1] - r[0]);
    }
    else if (raw >= r[last])
    {
        mm = t[last] + (raw - r[last]) * TB_CAL_EXTRA_GAIN[id];
    }
    else
    {
        int i = 0;
        while (i < last - 1 && raw > r[i + 1]) i++;
        float f = (raw - r[i]) / (r[i + 1] - r[i]);
        mm = t[i] + f * (t[i + 1] - t[i]);
    }
    return mm;
}

uint16_t tb_tofProcess(uint8_t address, uint16_t raw)
{
    int id = (int)address - TOF1_ADDR;
    if (id < 0 || id >= TB_TOF_COUNT) return raw;

    tbTofRaw[id] = raw;
    if (raw == 65535) return raw;                 // timeout: sensors.cpp reports 8190

    uint16_t v = raw;
    if (g_live.tofMedianFilter)
    {
        tbTofHist[id][tbTofHistI[id]] = raw;
        tbTofHistI[id] = (uint8_t)((tbTofHistI[id] + 1) % 3);
        if (tbTofHistN[id] < 3) tbTofHistN[id]++;
        if (tbTofHistN[id] == 3) v = tbMedian3(tbTofHist[id][0], tbTofHist[id][1], tbTofHist[id][2]);
    }
    else
    {
        tbTofHistN[id] = 0;
        tbTofHistI[id] = 0;
    }

    if (v == 0 || v >= TOF_VALID_MAX_MM)          // nothing valid: pass through
    {
        tbTofCal[id] = 8190;
        return v;
    }
    if (!g_live.tofUseCalibration)
    {
        tbTofCal[id] = v;
        return v;
    }

    float mm = tbCalibrate(id, (float)v) + g_live.tofTrimMm[id];
    if (mm < 1.0f) mm = 1.0f;                     // 0 would mean "no reading" to sensors.cpp
    if (mm > 8000.0f) mm = 8000.0f;
    uint16_t out = (uint16_t)lroundf(mm);
    tbTofCal[id] = out;
    return out;
}

// ============================================================================
//  TRACE
// ============================================================================
static void tbTraceMaybeSample()
{
    if (!tbTraceOn) return;
    uint32_t now = micros();
    if (tbTraceCount > 0 && (uint32_t)(now - tbTraceLastUs) < 5000u) return;
    tbTraceLastUs = now;

    TbTraceSample &s = tbTrace[tbTraceHead];
    s.tMs     = millis() - tbTraceT0;
    s.encL    = (int32_t)motors_leftEncoder();
    s.encR    = (int32_t)motors_rightEncoder();
    s.yawCdeg = tbClamp16(tbStep.yaw * 100.0f);
    s.latDmm  = tbClamp16(tbStep.lat * 10.0f);
    s.angCdeg = tbClamp16(tbStep.ang * 100.0f);
    s.pwmL    = (int16_t)tbTelPwmL;
    s.pwmR    = (int16_t)tbTelPwmR;
    s.front   = tbStep.front;
    s.lMin    = tbStep.lMin;
    s.rMin    = tbStep.rMin;
    s.step    = tbTraceStepNo;
    s.flags   = (uint8_t)((tbStep.leftWall ? 1 : 0) | (tbStep.rightWall ? 2 : 0) | (tbTelBraking ? 4 : 0));
    tbTraceHead = (tbTraceHead + 1) % TB_TRACE_MAX;
    if (tbTraceCount < TB_TRACE_MAX) tbTraceCount++;
}

#if TUNER_WIFI
static void tbTraceStart()
{
    tbTraceOn = false;
    tbTraceHead = 0;
    tbTraceCount = 0;
    tbTraceT0 = millis();
    tbTraceLastUs = 0;
    tbTraceStepNo = 0;
    tbTraceOn = true;
}

static void tbTraceStop()
{
    tbTraceOn = false;
}
#endif

static void tbStepReset()
{
    memset(&tbStep, 0, sizeof(tbStep));
    tbStep.t0Ms = millis();
    tbStep.front = 8190;
    tbStep.lMin = 8190;
    tbStep.rMin = 8190;
    tbStep.lMinSeen = 8190;
    tbStep.rMinSeen = 8190;
}

// ============================================================================
//  HOOKS FOR motors.cpp  (see tuner_bridge.h)
// ============================================================================
float tb_hookYaw()
{
    float yaw = sensors_yawDegrees();
    uint32_t now = micros();
    if (tbStep.lastYawUs != 0)
    {
        float dt = (float)(uint32_t)(now - tbStep.lastYawUs) * 1.0e-6f;
        if (dt > 0.0005f && dt < 0.1f)
        {
            float rate = (yaw - tbStep.yaw) / dt;
            tbTelRate = rate;
            if (fabsf(rate) > tbStep.peakRate) tbStep.peakRate = fabsf(rate);
        }
    }
    tbStep.lastYawUs = now;
    tbStep.yaw = yaw;
    tbTelYaw = yaw;
    return yaw;
}

void tb_hookResetYaw()
{
    sensors_resetYaw();
    tbStepReset();                  // motors.cpp starts every move and turn with this
    tbTraceStepNo++;

    // New move or turn: forget the ToF median history. Otherwise the first
    // reading after a turn is outvoted by two samples taken facing the old way
    // (e.g. the wall that was ahead), which made floodfill record walls that
    // are not there ("wall ahead after turn") and miss walls that are.
    memset(tbTofHistN, 0, sizeof(tbTofHistN));
    memset(tbTofHistI, 0, sizeof(tbTofHistI));
}

void tb_hookSideGuidance(SideGuidance *g)
{
    sensors_readSideGuidance(g);
    if (g == NULL) return;
    tbStep.lat = g->lateralErrorMm;
    tbStep.ang = g->angleErrorDeg;
    tbStep.lMin = g->leftMinMm;
    tbStep.rMin = g->rightMinMm;
    if (g->leftMinMm < tbStep.lMinSeen) tbStep.lMinSeen = g->leftMinMm;
    if (g->rightMinMm < tbStep.rMinSeen) tbStep.rMinSeen = g->rightMinMm;
    tbStep.leftWall = g->leftWall;
    tbStep.rightWall = g->rightWall;
    tbTelLat = g->lateralErrorMm;
    tbTelAng = g->angleErrorDeg;
    tbTelLMin = g->leftMinMm;
    tbTelRMin = g->rightMinMm;
    tbTelWallL = g->leftWall ? 1 : 0;
    tbTelWallR = g->rightWall ? 1 : 0;
}

uint16_t tb_hookFront()
{
    uint16_t f = sensors_frontDistanceMm();
    tbStep.front = f;
    tbTelFront = f;
    tbTelWallF = (f > 0 && f < TOF_VALID_MAX_MM && (int)f < g_live.frontWallThresholdMm) ? 1 : 0;
    return f;
}

void tb_hookDelay(uint32_t ms)
{
    tbTraceMaybeSample();
    delay(ms);
}

static int tbSignedPwm(int in1, int in2, int value, int inverted)
{
    bool a = tbPinLevel[in1] != 0;
    bool b = tbPinLevel[in2] != 0;
    if (a && b)
    {
        tbTelBraking = true;
        return 0;
    }
    tbTelBraking = false;
    int sign = 0;
    if (a && !b) sign = 1;
    else if (!a && b) sign = -1;
    if (inverted) sign = -sign;
    return sign * value;
}

void tb_hookDigitalWrite(uint8_t pin, uint8_t level)
{
    if (pin < sizeof(tbPinLevel)) tbPinLevel[pin] = level;
    digitalWrite(pin, level);
}

void tb_hookAnalogWrite(uint8_t pin, int value)
{
    analogWrite(pin, value);
    if (pin == PIN_PWMA) tbTelPwmL = tbSignedPwm(PIN_AIN1, PIN_AIN2, value, g_live.motorAInverted);
    else if (pin == PIN_PWMB) tbTelPwmR = tbSignedPwm(PIN_BIN1, PIN_BIN2, value, g_live.motorBInverted);
}

// ============================================================================
//  WHY DID A MOVE / TURN FAIL?  (from what the hooks saw)
// ============================================================================
static int tbMoveCells = 1;          // cells in the move now running (multi-cell fast runs)

static void tbExplainMove(char *out, size_t n)
{
    uint32_t el = millis() - tbStep.t0Ms;
    float mm = tbAvgMm();
    if (g_tbAbort)
        snprintf(out, n, "stopped from the tuner");
    else if (el >= 3700UL * (uint32_t)tbMoveCells)
        snprintf(out, n, "timeout: %.1f s for %.0f mm (stall?)", 3.8 * tbMoveCells, (double)mm);
    else if (mm > 18.0f && (int)tbStep.front < g_live.frontEmergencyMm)
        snprintf(out, n, "front emergency: %u mm < %d", (unsigned)tbStep.front, g_live.frontEmergencyMm);
    else if ((int)tbStep.lMin < g_live.sideHardStopMm)
        snprintf(out, n, "side hard stop: left %u mm < %d", (unsigned)tbStep.lMin, g_live.sideHardStopMm);
    else if ((int)tbStep.rMin < g_live.sideHardStopMm)
        snprintf(out, n, "side hard stop: right %u mm < %d", (unsigned)tbStep.rMin, g_live.sideHardStopMm);
    else
        snprintf(out, n, "move failed after %.0f mm", (double)mm);
}

static void tbExplainTurn(char *out, size_t n, bool physicalLeft)
{
    uint32_t el = millis() - tbStep.t0Ms;
    float progress = physicalLeft ? tbStep.yaw : -tbStep.yaw;
    if (g_tbAbort)
        snprintf(out, n, "stopped from the tuner");
    else if (progress < -12.0f)
        snprintf(out, n, "wrong way (%.0f deg): check MOTOR_x_INVERTED / GYRO_Z_INVERTED", (double)progress);
    else if (el >= (uint32_t)g_live.turnTimeoutMs)
        snprintf(out, n, "turn timeout: %.0f deg in %u ms (slow PWM too low?)", (double)progress, (unsigned)el);
    else
        snprintf(out, n, "turn failed at %.0f deg", (double)progress);
}

// ============================================================================
//  MAZE TRACKER + HOOKS FOR API.cpp
//  floodfill works in a logical frame: start (0,0) facing north with the maze
//  to the right.  When the real maze opens to the LEFT, left and right are
//  swapped here, so floodfill.cpp and API.cpp stay unchanged.
// ============================================================================
static void tbLogAdd(const char *fmt, ...)
{
    char buf[TB_LOG_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    tbLockTake();
    memcpy(tbLog[tbLogCount % TB_LOG_N], buf, TB_LOG_LEN);
    tbLogCount++;
    tbLockGive();
    Serial.print("[maze] ");
    Serial.println(buf);
}

static const char *tbDirName(uint8_t h)
{
    static const char *const names[4] = { "N", "E", "S", "W" };
    return names[h & 3];
}

static void tbMazeReset()
{
    tbLockTake();
    tbMzX = 0;
    tbMzY = 0;
    tbMzH = 0;
    memset(tbMzWalls, 0, sizeof(tbMzWalls));  // the page draws the MAZE_SIZE border itself
    tbMzMoves = 0;
    tbMzTurns = 0;
    tbMzT0 = millis();
    tbMzSensedF = tbMzSensedR = false;
    tbMzAfterTurn = false;
    tbLogCount = 0;
    tbMzFault[0] = '\0';
    tbLockGive();

    int opens = g_live.mazeOpens;
    tbMzMirror = (opens == 1) ? 1 : (opens == 2 ? -1 : 0);
}

static void tbMazeRecord(uint8_t dir, bool wall)
{
    int x = tbMzX;
    int y = tbMzY;
    if (x < 0 || x > 15 || y < 0 || y > 15) return;
    dir &= 3;
    uint8_t known = (uint8_t)(1 << dir);
    uint8_t bit = (uint8_t)(16 << dir);
    tbMzWalls[x][y] |= known;
    if (wall) tbMzWalls[x][y] |= bit; else tbMzWalls[x][y] &= (uint8_t)~bit;

    int nx = x + TB_DX[dir];
    int ny = y + TB_DY[dir];
    if (nx < 0 || nx > 15 || ny < 0 || ny > 15) return;
    uint8_t od = (uint8_t)((dir + 2) & 3);
    tbMzWalls[nx][ny] |= (uint8_t)(1 << od);
    if (wall) tbMzWalls[nx][ny] |= (uint8_t)(16 << od); else tbMzWalls[nx][ny] &= (uint8_t)~(16 << od);
}

// AUTO: decide the side at the first cell of the start column with exactly
// one physical side open (the other side is the outer wall).
static void tbMazeResolveMirror()
{
    if (tbMzMirror >= 0) return;
    bool physLeft = sensors_wallLeft();
    bool physRight = sensors_wallRight();
    if (physLeft == physRight) return;
    tbMzMirror = physLeft ? 0 : 1;
    tbLogAdd("AUTO: maze opens to the %s", tbMzMirror ? "LEFT (mirrored)" : "RIGHT");
}

// Floodfill reads the walls right after the mouse stopped. The ToF readings
// taken while it was still moving or turning are thrown away (a sensor that was
// not read during a turn hands back a measurement from before or during it),
// and the median filter is filled with readings taken standing still.
// About 60 ms per cell.
static void tbReadAllToF()
{
    sensors_frontDistanceMm();
    sensors_leftFrontDistanceMm();
    sensors_leftRearDistanceMm();
    sensors_rightFrontDistanceMm();
    sensors_rightRearDistanceMm();
}

static void tbFreshToF()
{
    tbReadAllToF();                                   // old measurements: thrown away
    memset(tbTofHistN, 0, sizeof(tbTofHistN));
    memset(tbTofHistI, 0, sizeof(tbTofHistI));
    tbReadAllToF();                                   // two new ones; the wall check
    tbReadAllToF();                                   // itself reads the third
}

bool tb_hookWallFront()
{
    tbFreshToF();
    bool w = sensors_wallFront();
    tbMazeRecord(tbMzH, w);
    if (tbMzAfterTurn)
    {
        if (w) tbLogAdd("(%d,%d)%s wall ahead after turn", tbMzX, tbMzY, tbDirName(tbMzH));
        tbMzAfterTurn = false;
    }
    else
    {
        tbMzSensedF = w;
    }
    return w;
}

bool tb_hookWallRight()
{
    tbMazeResolveMirror();
    bool w = (tbMzMirror == 1) ? sensors_wallLeft() : sensors_wallRight();
    tbMazeRecord((uint8_t)(tbMzH + 1), w);
    tbMzSensedR = w;
    return w;
}

bool tb_hookWallLeft()
{
    tbMazeResolveMirror();
    bool w = (tbMzMirror == 1) ? sensors_wallRight() : sensors_wallLeft();
    tbMazeRecord((uint8_t)(tbMzH + 3), w);
    tbLogAdd("(%d,%d)%s walls F%d L%d R%d", tbMzX, tbMzY, tbDirName(tbMzH),
             tbMzSensedF ? 1 : 0, w ? 1 : 0, tbMzSensedR ? 1 : 0);
    return w;
}

bool tb_hookMoveOneCell()
{
    return tb_hookMoveCells(1);
}

// One cell (search, and fast runs cell by cell) or a whole straight in one go
// (fast runs with FAST_CONTINUOUS = 1).
bool tb_hookMoveCells(int cells)
{
    if (g_tbAbort) return false;
    if (cells < 1) cells = 1;
    tbMoveCells = cells;
    bool ok = (cells == 1) ? motors_moveOneCell() : motors_moveCells(cells);
    if (g_tbAbort) ok = false;
    if (ok)
    {
        if (cells > 1)
            tbLogAdd("(%d,%d)%s straight %d cells", tbMzX, tbMzY, tbDirName(tbMzH), cells);
        tbMzX += TB_DX[tbMzH] * cells;
        tbMzY += TB_DY[tbMzH] * cells;
        tbMzMoves += cells;
    }
    else
    {
        char why[80];
        tbExplainMove(why, sizeof(why));
        tbMoveCells = 1;
        if (g_tbAbort)
        {
            tbLogAdd("stopped at (%d,%d)%s", tbMzX, tbMzY, tbDirName(tbMzH));
            return false;
        }
        tbLogAdd("FAULT at (%d,%d)%s: %s", tbMzX, tbMzY, tbDirName(tbMzH), why);
        tbStatus("maze stopped: %s", why);
        snprintf(tbMzFault, sizeof(tbMzFault), "%s", why);
    }
    tbMoveCells = 1;
    return ok;
}

static bool tbMazeTurn(bool logicalRight)
{
    if (g_tbAbort) return false;
    bool physicalLeft = (tbMzMirror == 1) ? logicalRight : !logicalRight;
    bool ok = physicalLeft ? motors_turnLeft90() : motors_turnRight90();
    if (g_tbAbort) ok = false;
    if (ok)
    {
        tbMzH = (uint8_t)((tbMzH + (logicalRight ? 1 : 3)) & 3);
        tbMzTurns++;
        tbMzAfterTurn = true;
    }
    else
    {
        char why[80];
        tbExplainTurn(why, sizeof(why), physicalLeft);
        if (g_tbAbort)
        {
            tbLogAdd("stopped at (%d,%d)%s", tbMzX, tbMzY, tbDirName(tbMzH));
            return false;
        }
        tbLogAdd("FAULT turning at (%d,%d): %s", tbMzX, tbMzY, why);
        tbStatus("maze stopped: %s", why);
        snprintf(tbMzFault, sizeof(tbMzFault), "%s", why);
    }
    return ok;
}

bool tb_hookTurnRight90() { return tbMazeTurn(true); }
bool tb_hookTurnLeft90()  { return tbMazeTurn(false); }

// Maze size and goal for floodfill (API_mazeWidth/Height, API_goalX/Y).
// floodfill frame: x = cells sideways into the maze, y = cells forward.
int tb_mazeSize()
{
    return g_live.mazeSize ? 16 : 10;
}

static int tbGoalClamp(int v)
{
    int hi = tb_mazeSize() - 2;              // the 2x2 must fit inside the maze
    if (v < 0) return 0;
    if (v > hi) return hi;
    return v;
}

int tb_goalX()
{
    if (g_live.goalMode == 0) return tb_mazeSize() / 2 - 1;
    return tbGoalClamp(g_live.goalSideways);
}

int tb_goalY()
{
    if (g_live.goalMode == 0) return tb_mazeSize() / 2 - 1;
    return tbGoalClamp(g_live.goalForward);
}

int tb_fastContinuous()
{
    return g_live.fastContinuous ? 1 : 0;
}

// ============================================================================
//  COMMON SET-UP (both builds)
// ============================================================================
static void tbInitCore()
{
    tbApplyDefaults();
    if (tbLock == NULL) tbLock = xSemaphoreCreateMutex();
    tbStepReset();
    tbMazeReset();
}

// ############################################################################
//  RACE BUILD  (TUNER_WIFI 0): no radio. Waits in the start cell (buttons,
//  LED, start gesture), then one run start -> goal -> start per start. The
//  learned maze, the mode and both straight PWMs stay in flash (NVS "comp").
// ############################################################################
#if !TUNER_WIFI

#define COMP_NVS             "comp"
#define COMP_PWM_STEP        10       // SPEED short press
#define COMP_DEBOUNCE_MS     30
#define COMP_SPEED_LONG_MS   2000     // SPEED held this long: back to the tuned value
#define COMP_CLEAR_MS        5000     // BOOT held this long: erase the saved maze
#define COMP_HAND_MM         100      // front reading below this = hand in front
#define COMP_HAND_GONE_MM    150      // above this, or nothing in range = hand taken away
#define COMP_HAND_HOLD_MS    1000
#define COMP_START_DELAY_MS  1000
#define COMP_FRONT_POLL_MS   50
#define COMP_MAP_MAX         1400     // >= floodfill_mapBytes()

enum { COMP_IDLE, COMP_ARMED, COMP_COUNTDOWN, COMP_RUNNING };

static CompButton compBtnMode  = { PIN_BTN_MODE, false, false, 0, 0, false };
static CompButton compBtnSpeed = { PIN_BTN_SPEED, false, false, 0, 0, false };
static CompButton compBtnBoot  = { PIN_BTN_BOOT, false, false, 0, 0, false };

static int      compState = COMP_IDLE;
static bool     compFast = false;              // selected mode
static int      compSteps[2] = { 0, 0 };       // straight PWM steps above the tuned value: [0] SEARCH, [1] FAST
static int      compBasePwm = 0;               // STRAIGHT_PWM of the TUNED VALUES block
static uint32_t compHandSinceMs = 0;
static uint32_t compCountdownMs = 0;
static uint32_t compLastFrontMs = 0;
static bool     compClearDone = false;         // erased; BOOT not released yet
static unsigned char compMapSaved[COMP_MAP_MAX];   // what flash holds
static int8_t   compMirrorSaved = -1;

// ---- LED: one pattern at a time, the idle heartbeat in between ----
static uint16_t compLedSeq[32];                // on, off, on, off ... (ms)
static int      compLedLen = 0;
static int      compLedPos = 0;
static uint32_t compLedPosMs = 0;
static uint32_t compBeatZeroMs = 0;            // heartbeat restarts here, clear of the last pattern

static void compLedBlinks(int count, int onMs, int offMs)
{
    if (count > 16) count = 16;
    compLedLen = 0;
    for (int i = 0; i < count; i++)
    {
        compLedSeq[compLedLen++] = (uint16_t)onMs;
        compLedSeq[compLedLen++] = (uint16_t)offMs;
    }
    compLedPos = 0;
    compLedPosMs = millis();
    compBeatZeroMs = compLedPosMs + (uint32_t)count * (onMs + offMs) + 800;
}

static void compLedUpdate(uint32_t now)
{
    bool on = false;
    if (compState == COMP_IDLE && compBtnBoot.down && !compClearDone)
    {
        on = (now / 60) % 2;                   // BOOT held: fast flashing until the erase
    }
    else if (compLedPos < compLedLen)
    {
        while (compLedPos < compLedLen && now - compLedPosMs >= compLedSeq[compLedPos])
        {
            compLedPosMs += compLedSeq[compLedPos];
            compLedPos++;
        }
        on = compLedPos < compLedLen && (compLedPos % 2) == 0;
    }
    else if (compState == COMP_IDLE && (int32_t)(now - compBeatZeroMs) >= 0)
    {
        uint32_t t = (now - compBeatZeroMs) % 2000;   // heartbeat: 1 blink = SEARCH, 2 = FAST
        uint32_t blinks = compFast ? 2 : 1;
        on = t < blinks * 300 && (t % 300) < 120;
    }
    else if (compState == COMP_ARMED)
    {
        on = true;                             // hand seen: take it away to start
    }
    digitalWrite(PIN_LED, on ? HIGH : LOW);
}

// ---- buttons: debounced, acted on at release ----
static void compButtonInit(CompButton &b, uint32_t now)
{
    pinMode(b.pin, INPUT_PULLUP);
    delay(2);
    b.raw = b.down = (digitalRead(b.pin) == LOW);
    b.rawSinceMs = b.downSinceMs = now;
    b.fromIdle = false;                        // already held: its release does nothing
}

// +1 pressed, -1 released (*heldMs = how long it was down), 0 nothing new.
static int compButtonEvent(CompButton &b, uint32_t now, uint32_t *heldMs)
{
    bool raw = (digitalRead(b.pin) == LOW);
    if (raw != b.raw)
    {
        b.raw = raw;
        b.rawSinceMs = now;
    }
    if (raw == b.down || now - b.rawSinceMs < COMP_DEBOUNCE_MS) return 0;
    b.down = raw;
    if (raw)
    {
        b.downSinceMs = now;
        b.fromIdle = (compState == COMP_IDLE);
        return 1;
    }
    *heldMs = now - b.downSinceMs;
    return -1;
}

// ---- settings and maze in flash ----
static int compPwm(bool fast)
{
    return compBasePwm + COMP_PWM_STEP * compSteps[fast ? 1 : 0];
}

static void compSaveSettings()
{
    Preferences p;
    p.begin(COMP_NVS, false);
    p.putUChar("mode", compFast ? 1 : 0);
    p.putUChar("stepS", (uint8_t)compSteps[0]);
    p.putUChar("stepF", (uint8_t)compSteps[1]);
    p.end();
}

// Writes the maze (and the AUTO start-side decision) only when it changed.
static void compSaveMaze()
{
    static unsigned char now[COMP_MAP_MAX];
    int n = floodfill_mapBytes();
    floodfill_exportMap(now);
    if (memcmp(now, compMapSaved, n) == 0 && tbMzMirror == compMirrorSaved) return;
    Preferences p;
    p.begin(COMP_NVS, false);
    p.putBytes("maze", now, n);
    p.putChar("mir", tbMzMirror);
    p.end();
    memcpy(compMapSaved, now, n);
    compMirrorSaved = tbMzMirror;
}

static int compCellsExplored()
{
    int n = floodfill_mapBytes();
    int cells = 0;
    for (int k = n - 256; k < n; k++) cells += compMapSaved[k];   // the map ends with 16 x 16 visited flags
    return cells;
}

static void compLoad()
{
    int n = floodfill_mapBytes();
    Preferences p;
    p.begin(COMP_NVS, true);                   // nothing saved yet: every read gives its default
    compFast = p.getUChar("mode", 0) == 1;
    compSteps[0] = p.getUChar("stepS", 0);
    compSteps[1] = p.getUChar("stepF", 0);
    bool mazeOk = false;
    if (p.getBytesLength("maze") == (size_t)n)
    {
        p.getBytes("maze", compMapSaved, n);
        mazeOk = floodfill_importMap(compMapSaved, n);
        if (!mazeOk) Serial.println("Saved maze is for another maze size or goal: ignored (BOOT 5 s erases it).");
    }
    int8_t mirror = p.getChar("mir", -1);
    p.end();

    if (!mazeOk) floodfill_exportMap(compMapSaved);          // the empty map is the baseline
    if (mazeOk && g_live.mazeOpens == 2 && mirror >= 0) tbMzMirror = mirror;
    compMirrorSaved = tbMzMirror;

    int maxSteps = (g_live.maxStraightPwm - compBasePwm) / COMP_PWM_STEP;
    if (maxSteps < 0) maxSteps = 0;
    for (int m = 0; m < 2; m++) if (compSteps[m] > maxSteps) compSteps[m] = maxSteps;

    bool path = floodfill_knownPathToGoal();
    if (compFast && !path)
    {
        compFast = false;
        Serial.println("Saved mode was FAST but there is no known path to the goal: SEARCH.");
    }
    Serial.printf("Saved maze: %s, %d cells explored, known path to the goal: %s\n",
                  mazeOk ? "loaded" : "none", compCellsExplored(), path ? "yes" : "no");
    Serial.printf("Mode %s | straight PWM: SEARCH %d, FAST %d (tuned %d, max %d)\n",
                  compFast ? "FAST" : "SEARCH", compPwm(false), compPwm(true), compBasePwm, g_live.maxStraightPwm);
}

static void compClearMaze()
{
    floodfill_clearMap();
    tbMazeReset();                             // start side back to MAZE_OPENS (AUTO decides again)
    Preferences p;
    p.begin(COMP_NVS, false);
    p.remove("maze");
    p.remove("mir");
    p.end();
    floodfill_exportMap(compMapSaved);
    compMirrorSaved = tbMzMirror;
    compFast = false;
    compSaveSettings();
    Serial.println("CLEAR: saved maze erased. Mode forced to SEARCH, speeds kept.");
}

// ---- button actions (only while idle in the start cell) ----
static void compToggleMode()
{
    if (!compFast && !floodfill_knownPathToGoal())
    {
        compLedBlinks(5, 80, 80);
        Serial.println("MODE: FAST refused - no known path to the goal yet. Staying in SEARCH.");
        return;
    }
    compFast = !compFast;
    compSaveSettings();
    compLedBlinks(compFast ? 2 : 1, 250, 250);
    Serial.printf("MODE: %s (straight PWM %d)\n", compFast ? "FAST" : "SEARCH", compPwm(compFast));
}

static void compSpeed(bool reset)
{
    int m = compFast ? 1 : 0;
    const char *name = compFast ? "FAST" : "SEARCH";
    if (reset)
    {
        compSteps[m] = 0;
        compSaveSettings();
        compLedBlinks(1, 1000, 0);
        Serial.printf("SPEED: %s straight PWM back to the tuned %d\n", name, compPwm(compFast));
        return;
    }
    if (compPwm(compFast) + COMP_PWM_STEP > g_live.maxStraightPwm)
    {
        compLedBlinks(3, 50, 50);
        Serial.printf("SPEED: %s straight PWM stays %d - limit (MAX_STRAIGHT_PWM %d)\n",
                      name, compPwm(compFast), g_live.maxStraightPwm);
        return;
    }
    int before = compPwm(compFast);
    compSteps[m]++;
    compSaveSettings();
    compLedBlinks(compSteps[m], 250, 250);
    Serial.printf("SPEED: %s straight PWM %d -> %d (tuned %d + %d x %d)\n",
                  name, before, compPwm(compFast), compBasePwm, compSteps[m], COMP_PWM_STEP);
}

// ---- runs ----
static void compStartRun()
{
    if (compFast && !floodfill_knownPathToGoal())
    {
        compFast = false;
        compSaveSettings();
        compLedBlinks(5, 80, 80);
        compState = COMP_IDLE;
        Serial.println("START: no known path to the goal - switched to SEARCH, give the start gesture again.");
        return;
    }
    g_live.straightPwm = compPwm(compFast);
    tbMzX = 0;                                 // maze tracker: start cell, start heading
    tbMzY = 0;
    tbMzH = 0;
    tbMzMoves = 0;
    tbMzTurns = 0;
    tbMzAfterTurn = false;
    tbMzFault[0] = '\0';
    digitalWrite(PIN_LED, LOW);
    Serial.printf("RUN START: %s, straight PWM %d\n", compFast ? "FAST" : "SEARCH", g_live.straightPwm);
    floodfill_startRun(compFast);
    compState = COMP_RUNNING;
}

static void compRunStep()
{
    bool going = floodfill_step();
    compSaveMaze();                            // every cell: a crash or power-off keeps what was learned
    if (going) return;

    motors_stop();
    Serial.println(floodfill_runComplete() ? "RUN END: back at the start."
                                           : "RUN END: stopped early (fault or no path) - carry the mouse to the start.");
    Serial.printf("%d cells explored, known path to the goal: %s. Place the mouse in the start cell, then the start gesture.\n",
                  compCellsExplored(), floodfill_knownPathToGoal() ? "yes" : "no");
    uint32_t now = millis();
    compButtonInit(compBtnMode, now);          // presses during the run are ignored
    compButtonInit(compBtnSpeed, now);
    compButtonInit(compBtnBoot, now);
    compHandSinceMs = 0;
    compLedLen = 0;
    compState = COMP_IDLE;
}

static void compIdle(uint32_t now)
{
    uint32_t heldMode = 0, heldSpeed = 0, heldBoot = 0;
    int eMode = compButtonEvent(compBtnMode, now, &heldMode);
    int eSpeed = compButtonEvent(compBtnSpeed, now, &heldSpeed);
    int eBoot = compButtonEvent(compBtnBoot, now, &heldBoot);
    bool anyDown = compBtnMode.down || compBtnSpeed.down || compBtnBoot.down;

    if (compState == COMP_IDLE)
    {
        if (eMode < 0 && compBtnMode.fromIdle) compToggleMode();
        if (eSpeed < 0 && compBtnSpeed.fromIdle) compSpeed(heldSpeed >= COMP_SPEED_LONG_MS);
        if (eBoot > 0)
        {
            compClearDone = false;
            Serial.println("BOOT: keep holding 5 s to erase the saved maze");
        }
        if (eBoot < 0 && compBtnBoot.fromIdle && !compClearDone) Serial.println("BOOT released early: maze kept");
        if (compBtnBoot.down && compBtnBoot.fromIdle && !compClearDone &&
            now - compBtnBoot.downSinceMs >= COMP_CLEAR_MS)
        {
            compClearMaze();
            compClearDone = true;
            compLedBlinks(1, 1000, 0);
        }
    }
    else if (eMode > 0 || eSpeed > 0 || eBoot > 0)
    {
        compState = COMP_IDLE;                 // a button while arming: no start, no button action
        compHandSinceMs = 0;
        Serial.println("START cancelled (button pressed)");
    }

    // Start gesture: hand in front 1 s, take it away, start 1 s later.
    if (now - compLastFrontMs >= COMP_FRONT_POLL_MS)
    {
        compLastFrontMs = now;
        uint16_t f = sensors_frontDistanceMm();
        bool hand = f > 0 && f < COMP_HAND_MM;
        bool gone = f == 0 || f >= COMP_HAND_GONE_MM;
        if (compState == COMP_IDLE)
        {
            if (!hand || anyDown) compHandSinceMs = 0;
            else if (compHandSinceMs == 0) compHandSinceMs = now;
            else if (now - compHandSinceMs >= COMP_HAND_HOLD_MS)
            {
                compState = COMP_ARMED;
                Serial.println("START: hand seen - take it away to start");
            }
        }
        else if (compState == COMP_ARMED && gone)
        {
            compState = COMP_COUNTDOWN;
            compCountdownMs = now;
            Serial.println("START: starting in 1 s - hands off");
        }
        else if (compState == COMP_COUNTDOWN && hand)
        {
            compState = COMP_ARMED;
            Serial.println("START: hand back - take it away to start");
        }
    }
    if (compState == COMP_COUNTDOWN && now - compCountdownMs >= COMP_START_DELAY_MS)
    {
        compStartRun();
        return;
    }

    compLedUpdate(now);
}

void setup()
{
    tbInitCore();

    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("ROBOFEST micromouse starting...  (RACE build, WiFi off)");
    Serial.println("Hardware: ESP32 + TB6612FNG + 5x VL53L0X + MPU6050");

    if (!motors_begin())
    {
        Serial.println("ERROR: motor initialisation failed.");
        while (true) delay(1000);
    }

    Serial.println("Keep the mouse completely still: calibrating MPU6050...");

    if (!sensors_begin())
    {
        Serial.println("ERROR: ToF/MPU6050 initialisation failed.");
        motors_stop();
        while (true) delay(1000);
    }

    Serial.print("MPU6050 Z-gyro bias (deg/s): ");
    Serial.println(sensors_gyroBiasDegPerSec(), 4);

    Serial.print("Initial ToF mm [F LF LR RF RR]: ");
    Serial.print(sensors_frontDistanceMm()); Serial.print(' ');
    Serial.print(sensors_leftFrontDistanceMm()); Serial.print(' ');
    Serial.print(sensors_leftRearDistanceMm()); Serial.print(' ');
    Serial.print(sensors_rightFrontDistanceMm()); Serial.print(' ');
    Serial.println(sensors_rightRearDistanceMm());

    Serial.print("Maze opens to the: ");
    Serial.println(g_live.mazeOpens == 0 ? "RIGHT" : (g_live.mazeOpens == 1 ? "LEFT" : "AUTO"));

    Serial.printf("Maze %dx%d, goal %s: forward %d-%d, sideways %d-%d (cells from start)\n",
                  tb_mazeSize(), tb_mazeSize(), g_live.goalMode ? "MANUAL" : "CENTRE",
                  tb_goalY(), tb_goalY() + 1, tb_goalX(), tb_goalX() + 1);

    tbMazeReset();
    if (!floodfill_begin())
    {
        Serial.println("ERROR: flood-fill initialisation failed.");
        motors_stop();
        while (true) delay(1000);
    }

    // After sensors_begin(): it drives GPIO33 once as the spare ToF's XSHUT.
    uint32_t now = millis();
    compButtonInit(compBtnMode, now);
    compButtonInit(compBtnSpeed, now);
    compButtonInit(compBtnBoot, now);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    compBasePwm = g_live.straightPwm;
    if (floodfill_mapBytes() > COMP_MAP_MAX)
    {
        Serial.println("ERROR: COMP_MAP_MAX too small for the maze map.");
        while (true) delay(1000);
    }
    compLoad();

    Serial.println("Idle in the start cell. MODE: search/fast | SPEED: +10 (hold 2 s: tuned value) | "
                   "BOOT 5 s: erase maze | start: hand in front 1 s, then away.");
}

void loop()
{
    if (compState == COMP_RUNNING)
    {
        compRunStep();
        return;
    }
    compIdle(millis());
    delay(2);
}

#endif // !TUNER_WIFI

// ############################################################################
//  TUNER BUILD  (TUNER_WIFI 1)
// ############################################################################
#if TUNER_WIFI

static WebServer   tbServer(80);
static Preferences tbPrefs;

// ---------------------------------------------------------------------------
// Results (JSON built by the control loop, read by the web page)
// ---------------------------------------------------------------------------
static void tbResAppend(const char *fmt, ...)
{
    if (tbResLen >= TB_RES_MAX - 1) return;
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(tbRes + tbResLen, TB_RES_MAX - tbResLen, fmt, ap);
    va_end(ap);
    if (w > 0) tbResLen += (size_t)w;
    if (tbResLen > TB_RES_MAX - 1) tbResLen = TB_RES_MAX - 1;
}

static void tbResBegin(const char *type)
{
    tbResLen = 0;
    tbResFirst = true;
    tbResAppend("{\"type\":\"%s\",\"rows\":[", type);
}

static void tbResRowStart()
{
    tbResAppend(tbResFirst ? "{" : ",{");
    tbResFirst = false;
}

static void tbJsonEscape(char *out, size_t n, const char *in)
{
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 2 < n; i++)
    {
        char c = in[i];
        if (c == '"' || c == '\\') { out[j++] = '\\'; out[j++] = c; }
        else if ((uint8_t)c < 0x20) { out[j++] = ' '; }
        else out[j++] = c;
    }
    out[j] = '\0';
}

static void tbResEnd(const char *note)
{
    char esc[200];
    tbJsonEscape(esc, sizeof(esc), note);
    tbResAppend("],\"note\":\"%s\"}", esc);
    tbLockTake();
    memcpy(tbResShared, tbRes, tbResLen + 1);
    tbResSeq++;
    tbLockGive();
}

// ---------------------------------------------------------------------------
// Raw motor drive for the checks that bypass motors.cpp (same pin logic)
// ---------------------------------------------------------------------------
static void tbDriveOne(int command, int in1, int in2, int pwmPin, bool inverted)
{
    if (command > 255) command = 255;
    if (command < -255) command = -255;
    if (inverted) command = -command;
    if (command > 0)
    {
        tb_hookDigitalWrite(in1, HIGH);
        tb_hookDigitalWrite(in2, LOW);
        tb_hookAnalogWrite(pwmPin, command);
    }
    else if (command < 0)
    {
        tb_hookDigitalWrite(in1, LOW);
        tb_hookDigitalWrite(in2, HIGH);
        tb_hookAnalogWrite(pwmPin, -command);
    }
    else
    {
        tb_hookAnalogWrite(pwmPin, 0);
        tb_hookDigitalWrite(in1, LOW);
        tb_hookDigitalWrite(in2, LOW);
    }
}

static void tbDrive(int left, int right)
{
    tbDriveOne(left, PIN_AIN1, PIN_AIN2, PIN_PWMA, g_live.motorAInverted != 0);
    tbDriveOne(right, PIN_BIN1, PIN_BIN2, PIN_PWMB, g_live.motorBInverted != 0);
}

static void tbBrake()
{
    tb_hookDigitalWrite(PIN_AIN1, HIGH);
    tb_hookDigitalWrite(PIN_AIN2, HIGH);
    tb_hookAnalogWrite(PIN_PWMA, 255);
    tb_hookDigitalWrite(PIN_BIN1, HIGH);
    tb_hookDigitalWrite(PIN_BIN2, HIGH);
    tb_hookAnalogWrite(PIN_PWMB, 255);
    delay(45);
    motors_stop();
}

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
static void tbSettle(uint32_t ms)
{
    uint32_t t0 = millis();
    while (millis() - t0 < ms)
    {
        if (tbSensorsOk) tb_hookYaw();
        tbTraceMaybeSample();
        delay(5);
    }
}

static bool tbCountdown(float seconds)
{
    uint32_t total = (uint32_t)(seconds * 1000.0f);
    uint32_t t0 = millis();
    while (millis() - t0 < total)
    {
        if (g_tbAbort) return false;
        float left = (float)(total - (millis() - t0)) / 1000.0f;
        tbStatus("starting in %.1f s - hands off", (double)left);
        if (tbSensorsOk) tb_hookYaw();
        delay(50);
    }
    return !g_tbAbort;
}

static void tbRowCell(int i, bool ok, uint32_t ms, const char *why)
{
    long l = labs(motors_leftEncoder());
    long r = labs(motors_rightEncoder());
    char esc[160];
    tbJsonEscape(esc, sizeof(esc), why);
    tbResRowStart();
    tbResAppend("\"i\":%d,\"c\":\"F\",\"ok\":%d,\"ms\":%u,\"mmL\":%.1f,\"mmR\":%.1f,\"yaw\":%.2f,"
                "\"lat\":%.1f,\"lmin\":%u,\"rmin\":%u,\"front\":%u,\"why\":\"%s\"}",
                i, ok ? 1 : 0, (unsigned)ms, (double)tbCountsToMm(l), (double)tbCountsToMm(r),
                (double)tbStep.yaw, (double)tbStep.lat, (unsigned)tbStep.lMinSeen,
                (unsigned)tbStep.rMinSeen, (unsigned)tbStep.front, esc);
}

static void tbRowTurn(int i, char c, bool ok, uint32_t ms, float deg, const char *why)
{
    char esc[160];
    tbJsonEscape(esc, sizeof(esc), why);
    tbResRowStart();
    tbResAppend("\"i\":%d,\"c\":\"%c\",\"ok\":%d,\"ms\":%u,\"deg\":%.1f,\"rate\":%.0f,\"why\":\"%s\"}",
                i, c, ok ? 1 : 0, (unsigned)ms, (double)deg, (double)tbStep.peakRate, esc);
}

// One forward cell through the real motors_moveOneCell(), then 150 ms at rest.
static bool tbDoCell(int i)
{
    uint32_t t0 = millis();
    bool ok = motors_moveOneCell();
    uint32_t ms = millis() - t0;
    if (g_tbAbort) ok = false;
    char why[80] = "";
    if (!ok) tbExplainMove(why, sizeof(why));
    tbSettle((uint32_t)g_live.cellStopMs);
    tbRowCell(i, ok, ms, why);
    return ok;
}

// One 90 degree turn through the real turn code; the angle is measured after
// 400 ms so the coast after the brake is included.
static bool tbDoTurn(int i, bool left)
{
    uint32_t t0 = millis();
    bool ok = left ? motors_turnLeft90() : motors_turnRight90();
    uint32_t ms = millis() - t0;
    if (g_tbAbort) ok = false;
    char why[80] = "";
    if (!ok) tbExplainTurn(why, sizeof(why), left);
    tbSettle(400);
    float deg = left ? tbStep.yaw : -tbStep.yaw;
    tbRowTurn(i, left ? 'L' : 'R', ok, ms, deg, why);
    return ok;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
static void tbTestDirection(int motor, int pwm, int ms)
{
    tbResBegin("dir");
    pwm = (int)tbClampf((float)pwm, 30, 200);
    ms = (int)tbClampf((float)ms, 100, 1500);
    motors_resetEncoders();
    tbStatus("%s motor forward at PWM %d for %d ms", motor == 0 ? "LEFT" : "RIGHT", pwm, ms);
    if (motor == 0) tbDrive(pwm, 0); else tbDrive(0, pwm);
    uint32_t t0 = millis();
    while (millis() - t0 < (uint32_t)ms && !g_tbAbort) delay(5);
    tbBrake();
    delay(200);
    long cl = motors_leftEncoder();
    long cr = motors_rightEncoder();
    tbResRowStart();
    tbResAppend("\"motor\":\"%s\",\"pwm\":%d,\"ms\":%d,\"cL\":%ld,\"cR\":%ld}",
                motor == 0 ? "LEFT" : "RIGHT", pwm, ms, cl, cr);
    tbResEnd("");
}

static void tbTestEncoderMove(float targetMm)
{
    tbResBegin("enc");
    targetMm = tbClampf(targetMm, 50, 2000);
    motors_resetEncoders();
    tbStepReset();
    uint32_t t0 = millis();
    bool timedOut = false;
    while (!g_tbAbort)
    {
        long l = labs(motors_leftEncoder());
        long r = labs(motors_rightEncoder());
        float avg = (tbCountsToMm(l) + tbCountsToMm(r)) * 0.5f;
        if (avg >= targetMm) break;
        if (millis() - t0 > 8000) { timedOut = true; break; }
        float remaining = targetMm - avg;
        int pwm = g_live.straightPwm;
        if (remaining < 40.0f)
            pwm = g_live.straightMinPwm + (int)((g_live.straightPwm - g_live.straightMinPwm) * (remaining / 40.0f));
        int corr = (int)(g_live.kpEncoderSync * (float)(l - r));
        int lp = pwm - corr;
        int rp = pwm + corr;
        if (lp < g_live.straightMinPwm) lp = g_live.straightMinPwm;
        if (rp < g_live.straightMinPwm) rp = g_live.straightMinPwm;
        if (lp > 255) lp = 255;
        if (rp > 255) rp = 255;
        tbDrive(lp, rp);
        tbStatus("encoder move %.0f / %.0f mm", (double)avg, (double)targetMm);
        tbTraceMaybeSample();
        delay(2);
    }
    tbBrake();
    tbSettle(300);
    long cl = labs(motors_leftEncoder());
    long cr = labs(motors_rightEncoder());
    tbLastEncAvgCounts = (float)(cl + cr) * 0.5f;
    tbResRowStart();
    tbResAppend("\"cL\":%ld,\"cR\":%ld,\"avg\":%.1f,\"mm\":%.1f,\"target\":%.0f,\"timeout\":%d}",
                cl, cr, (double)tbLastEncAvgCounts, (double)tbCountsToMm((long)tbLastEncAvgCounts),
                (double)targetMm, timedOut ? 1 : 0);
    tbResEnd(g_tbAbort ? "stopped" : (timedOut ? "timeout: wheels never reached the distance" :
                                                  "now measure the real distance and enter it"));
}

static void tbCalibrateCpr(float measuredMm)
{
    tbResBegin("cpr");
    if (tbLastEncAvgCounts <= 0.0f || measuredMm < 10.0f)
    {
        tbResEnd("run the encoder move first, then enter the ruler distance");
        return;
    }
    int i = tbFindParam("ENCODER_COUNTS_PER_REV");
    float before = g_live.encoderCountsPerRev;
    float cpr = tbLastEncAvgCounts * PI * g_live.wheelDiameterMm / measuredMm;
    float now = tbSetParam(tbParams[i], cpr);
    tbDirty = true;
    tbResRowStart();
    tbResAppend("\"before\":%.1f,\"after\":%.1f,\"mm\":%.1f}", (double)before, (double)now, (double)measuredMm);
    tbResEnd("ENCODER_COUNTS_PER_REV updated. Repeat the move to confirm, then Save.");
}

static void tbTestStraight(int n)
{
    tbResBegin("straight");
    n = (int)tbClampf((float)n, 1, 16);
    for (int i = 1; i <= n && !g_tbAbort; i++)
    {
        tbStatus("cell %d of %d", i, n);
        if (!tbDoCell(i)) break;
    }
    motors_stop();
    tbResEnd(g_tbAbort ? "stopped" : "");
}

static void tbTestTurns(char kind, int n)
{
    tbResBegin("turns");
    bool left = (kind == 'L');
    if (kind == 'U') n = 2;                   // floodfill makes a U-turn from two right turns
    n = (int)tbClampf((float)n, 1, 12);
    float total = 0;
    int done = 0;
    for (int i = 1; i <= n && !g_tbAbort; i++)
    {
        tbStatus("turn %d of %d", i, n);
        bool ok = tbDoTurn(i, left);
        if (!ok) break;
        total += left ? tbStep.yaw : -tbStep.yaw;
        done++;
        tbSettle(150);
    }
    motors_stop();
    char note[96];
    snprintf(note, sizeof(note), "%d turn(s), total %.1f deg (target %d)", done, (double)total, done * 90);
    tbResEnd(g_tbAbort ? "stopped" : note);
}

static void tbTestSequence(const char *seq)
{
    tbResBegin("seq");
    int i = 0;
    for (const char *p = seq; *p != '\0' && !g_tbAbort && i < 40; p++)
    {
        char c = (char)toupper((unsigned char)*p);
        bool ok = true;
        if (c == 'F')
        {
            i++;
            tbStatus("sequence step %d: forward", i);
            ok = tbDoCell(i);
        }
        else if (c == 'L' || c == 'R')
        {
            i++;
            tbStatus("sequence step %d: turn %c", i, c);
            ok = tbDoTurn(i, c == 'L');
            tbSettle(100);
        }
        else if (c == 'U')
        {
            i++;
            tbStatus("sequence step %d: U-turn", i);
            ok = tbDoTurn(i, false);
            if (ok) { tbSettle(100); i++; ok = tbDoTurn(i, false); }
            tbSettle(100);
        }
        else
        {
            continue;
        }
        if (!ok) break;
    }
    motors_stop();
    tbResEnd(g_tbAbort ? "stopped" : "");
}

// Front open -> forward, else left, else right, else U-turn (same as the test sketch's 'w').
static void tbTestFollow(int steps)
{
    tbResBegin("seq");
    steps = (int)tbClampf((float)steps, 1, 60);
    int i = 0;
    for (int s = 0; s < steps && !g_tbAbort; s++)
    {
        bool f = sensors_wallFront();
        bool l = sensors_wallLeft();
        bool r = sensors_wallRight();
        bool ok;
        i++;
        if (!f)      { tbStatus("follow %d: forward", s + 1); ok = tbDoCell(i); }
        else if (!l) { tbStatus("follow %d: left", s + 1);    ok = tbDoTurn(i, true); }
        else if (!r) { tbStatus("follow %d: right", s + 1);   ok = tbDoTurn(i, false); }
        else
        {
            tbStatus("follow %d: dead end, U-turn", s + 1);
            ok = tbDoTurn(i, false);
            if (ok) { tbSettle(100); i++; ok = tbDoTurn(i, false); }
        }
        if (!ok) break;
        tbSettle(100);
    }
    motors_stop();
    tbResEnd(g_tbAbort ? "stopped" : "");
}

static void tbTestMaze()
{
    tbResBegin("maze");
    tbMazeReset();
    tbLogAdd("maze run started, MAZE_OPENS=%s",
             g_live.mazeOpens == 0 ? "RIGHT" : (g_live.mazeOpens == 1 ? "LEFT" : "AUTO"));
    if (!floodfill_begin())
    {
        tbResEnd("floodfill_begin failed");
        return;
    }
    uint32_t t0 = millis();
    int steps = 0;
    while (!g_tbAbort)
    {
        if (!floodfill_step()) break;
        steps++;
        tbStatus("(%d,%d) %s, %u cells, %u turns, %lu s", tbMzX, tbMzY, tbDirName(tbMzH),
                 (unsigned)tbMzMoves, (unsigned)tbMzTurns, (unsigned long)((millis() - t0) / 1000));
    }
    motors_stop();
    uint32_t s = (millis() - t0) / 1000;
    tbResRowStart();
    tbResAppend("\"steps\":%d,\"moves\":%u,\"turns\":%u,\"s\":%lu,\"x\":%d,\"y\":%d}",
                steps, (unsigned)tbMzMoves, (unsigned)tbMzTurns, (unsigned long)s, tbMzX, tbMzY);
    char note[160];
    if (g_tbAbort)
        snprintf(note, sizeof(note), "stopped from the tuner after %lu s, %u cells", (unsigned long)s, (unsigned)tbMzMoves);
    else if (tbMzFault[0] != '\0')
        snprintf(note, sizeof(note), "FAULT after %lu s, %u cells: %s", (unsigned long)s, (unsigned)tbMzMoves, tbMzFault);
    else
        snprintf(note, sizeof(note), "program finished after %lu s, %u cells", (unsigned long)s, (unsigned)tbMzMoves);
    tbLogAdd("%s", note);
    tbResEnd(note);
}

static void tbTestCapture(int samples)
{
    tbResBegin("capture");
    samples = (int)tbClampf((float)samples, 5, 200);
    float sumCal[TB_TOF_COUNT] = { 0 };
    float sumRaw[TB_TOF_COUNT] = { 0 };
    int   n[TB_TOF_COUNT] = { 0 };
    uint16_t lo[TB_TOF_COUNT], hi[TB_TOF_COUNT];
    for (int k = 0; k < TB_TOF_COUNT; k++) { lo[k] = 65535; hi[k] = 0; }

    for (int s = 0; s < samples && !g_tbAbort; s++)
    {
        uint16_t v[TB_TOF_COUNT];
        v[0] = sensors_frontDistanceMm();
        v[1] = sensors_leftFrontDistanceMm();
        v[2] = sensors_leftRearDistanceMm();
        v[3] = sensors_rightFrontDistanceMm();
        v[4] = sensors_rightRearDistanceMm();
        for (int k = 0; k < TB_TOF_COUNT; k++)
        {
            if (v[k] == 0 || v[k] >= TOF_VALID_MAX_MM) continue;
            sumCal[k] += v[k];
            sumRaw[k] += tbTofRaw[k];
            n[k]++;
            if (v[k] < lo[k]) lo[k] = v[k];
            if (v[k] > hi[k]) hi[k] = v[k];
        }
        tbStatus("capture %d / %d", s + 1, samples);
    }
    for (int k = 0; k < TB_TOF_COUNT; k++)
    {
        tbResRowStart();
        if (n[k] == 0)
            tbResAppend("\"s\":\"%s\",\"n\":0}", TB_TOF_NAMES[k]);
        else
            tbResAppend("\"s\":\"%s\",\"n\":%d,\"cal\":%.1f,\"raw\":%.1f,\"min\":%u,\"max\":%u}",
                        TB_TOF_NAMES[k], n[k], (double)(sumCal[k] / n[k]), (double)(sumRaw[k] / n[k]),
                        (unsigned)lo[k], (unsigned)hi[k]);
    }
    tbResEnd(g_tbAbort ? "stopped" : "");
}

// ---------------------------------------------------------------------------
// Flash (NVS) save / load, and import from the test sketch
// ---------------------------------------------------------------------------
static bool tbNvsLoad()
{
    if (!tbPrefs.begin("tuner", true)) return false;
    bool ok = false;
    uint32_t sig = tbPrefs.getUInt("sig", 0);
    size_t len = tbPrefs.getBytesLength("vals");
    if (sig == tbDefaultsSignature() && len == sizeof(float) * (size_t)TB_NPARAMS)
    {
        static float vals[TB_NPARAMS];
        tbPrefs.getBytes("vals", vals, len);
        for (int i = 0; i < TB_NPARAMS; i++) tbSetParam(tbParams[i], vals[i]);
        ok = true;
    }
    tbPrefs.end();
    return ok;
}

static void tbNvsSave()
{
    static float vals[TB_NPARAMS];
    for (int i = 0; i < TB_NPARAMS; i++) vals[i] = tbGetParam(tbParams[i]);
    tbPrefs.begin("tuner", false);
    tbPrefs.putUInt("sig", tbDefaultsSignature());
    tbPrefs.putBytes("vals", vals, sizeof(vals));
    tbPrefs.end();
    tbDirty = false;
    tbLoadedFromFlash = true;
}

static void tbNvsClear()
{
    tbPrefs.begin("tuner", false);
    tbPrefs.clear();
    tbPrefs.end();
    tbLoadedFromFlash = false;
}

// The test sketch (mouse-tests.ino) saved its live values in namespace "mouse".
static void tbImportTestSketch()
{
    static const char *const keys[] = { "cpr", "pwm", "minpwm", "kenc", "kgyro", "turnpwm", "slowpwm",
                                        "slowdeg", "stopdeg", "stopdeg", "timeout", "klat", "kang", "kdanger" };
    static const char *const ids[]  = { "ENCODER_COUNTS_PER_REV", "STRAIGHT_PWM", "STRAIGHT_MIN_PWM",
                                        "KP_ENCODER_SYNC", "KP_GYRO_HEADING", "TURN_PWM", "TURN_SLOW_PWM",
                                        "TURN_SLOW_START_DEG", "TURN_STOP_DEG_LEFT", "TURN_STOP_DEG_RIGHT",
                                        "TURN_TIMEOUT_MS", "KP_SIDE_LATERAL", "KP_SIDE_ANGLE", "KP_SIDE_DANGER" };
    tbResBegin("import");
    int got = 0;
    if (tbPrefs.begin("mouse", true))
    {
        for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++)
        {
            if (!tbPrefs.isKey(keys[k])) continue;
            int i = tbFindParam(ids[k]);
            if (i < 0) continue;
            float v = tbPrefs.getFloat(keys[k], tbGetParam(tbParams[i]));
            float now = tbSetParam(tbParams[i], v);
            tbResRowStart();
            tbResAppend("\"id\":\"%s\",\"v\":%.3f}", ids[k], (double)now);
            got++;
        }
        tbPrefs.end();
    }
    if (got > 0)
    {
        // the test sketch always used a 32 mm wheel with its counts per rev
        int w = tbFindParam("WHEEL_DIAMETER_MM");
        tbSetParam(tbParams[w], 32.0f);
        tbDirty = true;
    }
    tbResEnd(got > 0 ? "imported (wheel diameter set to 32 mm, as the test sketch used). Save to keep."
                     : "no values saved by the test sketch were found on this mouse");
}

// ---------------------------------------------------------------------------
// Command execution (control loop / core 1)
// ---------------------------------------------------------------------------
static bool tbNeedSensors()
{
    if (tbSensorsOk) return true;
    tbResBegin("error");
    tbResEnd("ToF / MPU6050 did not start: this test needs them. Check wiring, then reset.");
    return false;
}

static void tbExecute(const char *name, const char *text, const float *num)
{
    g_tbAbort = false;
    bool motion = true;
    float delaySec = tbClampf(num[3], 0, 10);

    if (strcmp(name, "zeroyaw") == 0)       { sensors_resetYaw(); tbStepReset(); motion = false; }
    else if (strcmp(name, "zeroenc") == 0)  { motors_resetEncoders(); motion = false; }
    else if (strcmp(name, "save") == 0)     { tbNvsSave(); tbResBegin("saved"); tbResEnd("saved to the mouse's flash"); motion = false; }
    else if (strcmp(name, "defaults") == 0)
    {
        tbApplyDefaults();
        tbNvsClear();
        tbDirty = false;
        tbResBegin("defaults");
        tbResEnd("back to the TUNED VALUES block; saved values erased");
        motion = false;
    }
    else if (strcmp(name, "import") == 0)   { tbImportTestSketch(); motion = false; }
    else if (strcmp(name, "calcpr") == 0)   { tbCalibrateCpr(num[0]); motion = false; }
    else if (strcmp(name, "capture") == 0)
    {
        motion = false;
        if (tbNeedSensors()) tbTestCapture((int)num[0]);
    }

    if (!motion)
    {
        tbStatus("ready");
        return;
    }

    // ---- tests that move the mouse ----
    bool needsSensors = !(strcmp(name, "dir") == 0 || strcmp(name, "enc") == 0);
    if (needsSensors && !tbNeedSensors()) return;
    if (!tbCountdown(delaySec))
    {
        tbResBegin("error");
        tbResEnd("stopped before start");
        tbStatus("ready");
        return;
    }

    tbTraceStart();
    if (strcmp(name, "dir") == 0)           tbTestDirection((int)num[0], (int)num[1], (int)num[2]);
    else if (strcmp(name, "enc") == 0)      tbTestEncoderMove(num[0]);
    else if (strcmp(name, "straight") == 0) tbTestStraight((int)num[0]);
    else if (strcmp(name, "turnL") == 0)    tbTestTurns('L', (int)num[0]);
    else if (strcmp(name, "turnR") == 0)    tbTestTurns('R', (int)num[0]);
    else if (strcmp(name, "uturn") == 0)    tbTestTurns('U', 2);
    else if (strcmp(name, "seq") == 0)      tbTestSequence(text);
    else if (strcmp(name, "follow") == 0)   tbTestFollow((int)num[0]);
    else if (strcmp(name, "maze") == 0)     tbTestMaze();
    else { tbResBegin("error"); tbResEnd("unknown test"); }
    tbTraceStop();
    motors_stop();
    tbStatus(g_tbAbort ? "stopped" : "ready");
}

// Idle: keep the live page fed (yaw every loop, ToFs every 100 ms).
static uint32_t tbLastTofMs = 0;

static void tbIdleTelemetry()
{
    if (!tbSensorsOk) return;
    tb_hookYaw();
    if (millis() - tbLastTofMs >= 100)
    {
        tbLastTofMs = millis();
        SideGuidance g;
        tb_hookSideGuidance(&g);
        tb_hookFront();
    }
}

static void tbService()
{
    char  name[sizeof(tbCmdName)];
    char  text[sizeof(tbCmdText)];
    float num[4];
    bool  have = false;

    tbLockTake();
    if (tbCmdPending)
    {
        memcpy(name, tbCmdName, sizeof(name));
        memcpy(text, tbCmdText, sizeof(text));
        memcpy(num, tbCmdNum, sizeof(num));
        tbCmdPending = false;
        tbBusy = true;
        snprintf(tbTask, sizeof(tbTask), "%s", name);
        have = true;
    }
    tbLockGive();

    if (have)
    {
        tbExecute(name, text, num);
        tbLockTake();
        tbBusy = false;
        snprintf(tbTask, sizeof(tbTask), "idle");
        tbLockGive();
        return;
    }

    tbIdleTelemetry();
    delay(4);
}

// ---------------------------------------------------------------------------
// Web server (runs in its own task on core 0; never touches I2C or motors)
// ---------------------------------------------------------------------------
static bool tbPost(const char *name, const char *text, float a, float b, float c, float d)
{
    bool ok = false;
    tbLockTake();
    if (!tbCmdPending && !tbBusy)
    {
        snprintf(tbCmdName, sizeof(tbCmdName), "%s", name);
        snprintf(tbCmdText, sizeof(tbCmdText), "%s", text);
        tbCmdNum[0] = a; tbCmdNum[1] = b; tbCmdNum[2] = c; tbCmdNum[3] = d;
        tbCmdPending = true;
        ok = true;
    }
    tbLockGive();
    return ok;
}

static float tbArgF(const char *name, float def)
{
    if (!tbServer.hasArg(name)) return def;
    String s = tbServer.arg(name);
    if (s.length() == 0) return def;
    return s.toFloat();
}

static void tbSendJson(const String &s)
{
    tbServer.sendHeader("Cache-Control", "no-store");
    tbServer.send(200, "application/json", s);
}

static void tbHttpPage()
{
    tbServer.sendHeader("Cache-Control", "no-store");
    tbServer.send_P(200, "text/html", TB_PAGE);
}

static void tbAppendJsonStr(String &s, const char *v)
{
    char esc[400];
    tbJsonEscape(esc, sizeof(esc), v);
    s += '"';
    s += esc;
    s += '"';
}

static void tbHttpParams()
{
    String s;
    s.reserve(12000);
    s += "{\"fw\":\"" TB_VERSION "\",\"flash\":";
    s += tbLoadedFromFlash ? "1" : "0";
    s += ",\"params\":[";
    for (int i = 0; i < TB_NPARAMS; i++)
    {
        const TbParam &p = tbParams[i];
        char num[120];
        if (i > 0) s += ',';
        s += "{\"id\":";   tbAppendJsonStr(s, p.id);
        s += ",\"tab\":";  tbAppendJsonStr(s, p.tab);
        s += ",\"grp\":";  tbAppendJsonStr(s, p.group);
        s += ",\"u\":";    tbAppendJsonStr(s, p.unit);
        s += ",\"h\":";    tbAppendJsonStr(s, p.help);
        snprintf(num, sizeof(num), ",\"k\":%d,\"v\":%g,\"d\":%g,\"lo\":%g,\"hi\":%g,\"st\":%g}",
                 p.kind, (double)tbGetParam(p), (double)p.def, (double)p.lo, (double)p.hi, (double)p.step);
        s += num;
    }
    s += "]}";
    tbSendJson(s);
}

static void tbHttpSet()
{
    String id = tbServer.arg("id");
    int i = tbFindParam(id.c_str());
    if (i < 0)
    {
        tbSendJson("{\"ok\":0,\"err\":\"unknown id\"}");
        return;
    }
    float v = tbSetParam(tbParams[i], tbArgF("v", tbGetParam(tbParams[i])));
    tbDirty = true;
    char buf[120];
    snprintf(buf, sizeof(buf), "{\"ok\":1,\"id\":\"%s\",\"v\":%g}", tbParams[i].id, (double)v);
    tbSendJson(buf);
}

static void tbHttpState()
{
    char msg[sizeof(tbMsg)];
    char task[sizeof(tbTask)];
    tbLockTake();
    memcpy(msg, tbMsg, sizeof(msg));
    memcpy(task, tbTask, sizeof(task));
    uint32_t resSeq = tbResSeq;
    bool busy = tbBusy || tbCmdPending;
    tbLockGive();
    char escMsg[200];
    tbJsonEscape(escMsg, sizeof(escMsg), msg);

    char buf[900];
    snprintf(buf, sizeof(buf),
             "{\"busy\":%d,\"task\":\"%s\",\"msg\":\"%s\",\"res\":%lu,\"abort\":%d,\"dirty\":%d,\"sens\":%d,"
             "\"tof\":[%u,%u,%u,%u,%u],\"raw\":[%u,%u,%u,%u,%u],\"wall\":[%d,%d,%d],"
             "\"lat\":%.1f,\"ang\":%.1f,\"yaw\":%.1f,\"rate\":%.0f,\"enc\":[%ld,%ld],\"pwm\":[%d,%d],"
             "\"lmin\":%u,\"rmin\":%u,\"up\":%lu,\"heap\":%lu,\"trace\":%d,\"log\":%u}",
             busy ? 1 : 0, task, escMsg, (unsigned long)resSeq, g_tbAbort ? 1 : 0, tbDirty ? 1 : 0,
             tbSensorsOk ? 1 : 0,
             (unsigned)tbTofCal[0], (unsigned)tbTofCal[1], (unsigned)tbTofCal[2], (unsigned)tbTofCal[3], (unsigned)tbTofCal[4],
             (unsigned)tbTofRaw[0], (unsigned)tbTofRaw[1], (unsigned)tbTofRaw[2], (unsigned)tbTofRaw[3], (unsigned)tbTofRaw[4],
             (int)tbTelWallF, (int)tbTelWallL, (int)tbTelWallR,
             (double)tbTelLat, (double)tbTelAng, (double)tbTelYaw, (double)tbTelRate,
             motors_leftEncoder(), motors_rightEncoder(), (int)tbTelPwmL, (int)tbTelPwmR,
             (unsigned)tbTelLMin, (unsigned)tbTelRMin,
             (unsigned long)(millis() / 1000), (unsigned long)ESP.getFreeHeap(), (int)tbTraceCount,
             (unsigned)tbLogCount);
    tbSendJson(buf);
}

static void tbHttpResult()
{
    String s;
    s.reserve(TB_RES_MAX + 32);
    tbLockTake();
    char head[32];
    snprintf(head, sizeof(head), "{\"seq\":%lu,\"r\":", (unsigned long)tbResSeq);
    s += head;
    s += tbResShared;
    tbLockGive();
    s += '}';
    tbSendJson(s);
}

static void tbHttpRun()
{
    String t = tbServer.arg("t");
    String seq = tbServer.arg("s");
    bool ok = tbPost(t.c_str(), seq.c_str(), tbArgF("n", 1), tbArgF("p", 90), tbArgF("ms", 400), tbArgF("d", 2));
    tbSendJson(ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
}

static void tbHttpStop()
{
    g_tbAbort = true;
    tbSendJson("{\"ok\":1}");
}

static void tbHttpCmd()
{
    String c = tbServer.arg("c");
    bool ok = tbPost(c.c_str(), "", tbArgF("n", 0), 0, 0, 0);
    tbSendJson(ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
}

static void tbHttpExport()
{
    static char buf[4096];
    tbBuildExport(buf, sizeof(buf));
    tbServer.sendHeader("Cache-Control", "no-store");
    tbServer.send(200, "text/plain", buf);
}

static void tbHttpTrace()
{
    tbServer.sendHeader("Cache-Control", "no-store");
    tbServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    tbServer.send(200, "text/csv", "");
    tbServer.sendContent("t_ms,step,encL,encR,yaw_deg,lat_mm,ang_deg,pwmL,pwmR,front,lmin,rmin,flags\n");
    int count = tbTraceCount;
    int start = (tbTraceHead - count + TB_TRACE_MAX) % TB_TRACE_MAX;
    char chunk[1600];
    size_t len = 0;
    for (int k = 0; k < count; k++)
    {
        const TbTraceSample &s = tbTrace[(start + k) % TB_TRACE_MAX];
        len += snprintf(chunk + len, sizeof(chunk) - len, "%lu,%u,%ld,%ld,%.2f,%.1f,%.2f,%d,%d,%u,%u,%u,%u\n",
                        (unsigned long)s.tMs, (unsigned)s.step, (long)s.encL, (long)s.encR,
                        s.yawCdeg / 100.0, s.latDmm / 10.0, s.angCdeg / 100.0, (int)s.pwmL, (int)s.pwmR,
                        (unsigned)s.front, (unsigned)s.lMin, (unsigned)s.rMin, (unsigned)s.flags);
        if (len > sizeof(chunk) - 120)
        {
            tbServer.sendContent(chunk);
            len = 0;
        }
    }
    if (len > 0) tbServer.sendContent(chunk);
    tbServer.sendContent("");
}

static void tbHttpMaze()
{
    String s;
    s.reserve(3600);
    char head[160];
    tbLockTake();
    snprintf(head, sizeof(head), "{\"x\":%d,\"y\":%d,\"h\":%d,\"mir\":%d,\"moves\":%u,\"turns\":%u,\"busy\":%d,"
             "\"n\":%d,\"gx\":%d,\"gy\":%d,\"w\":\"",
             tbMzX, tbMzY, tbMzH, tbMzMirror, (unsigned)tbMzMoves, (unsigned)tbMzTurns, tbBusy ? 1 : 0,
             tb_mazeSize(), tb_goalX(), tb_goalY());
    s += head;
    static const char hex[] = "0123456789abcdef";
    for (int y = 0; y < 16; y++)
    {
        for (int x = 0; x < 16; x++)
        {
            uint8_t v = tbMzWalls[x][y];
            s += hex[v >> 4];
            s += hex[v & 15];
        }
    }
    s += "\",\"log\":[";
    int n = tbLogCount < TB_LOG_N ? tbLogCount : TB_LOG_N;
    for (int k = 0; k < n; k++)
    {
        int idx = (tbLogCount - n + k) % TB_LOG_N;
        if (k > 0) s += ',';
        tbAppendJsonStr(s, tbLog[idx]);
    }
    tbLockGive();
    s += "]}";
    tbSendJson(s);
}

static void tbHttpNotFound()
{
    tbServer.send(404, "text/plain", "not found");
}

static void tbWebTask(void *arg)
{
    (void)arg;
    for (;;)
    {
        tbServer.handleClient();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

static void tbStartWifi()
{
    // Hotspot first (if one is set), else the mouse's own network.
    bool joined = false;
    if (strlen(TUNER_HOTSPOT_SSID) > 0)
    {
        WiFi.setHostname(TUNER_HOSTNAME);
        WiFi.mode(WIFI_STA);
        WiFi.setTxPower(WIFI_POWER_11dBm);
        WiFi.begin(TUNER_HOTSPOT_SSID, TUNER_HOTSPOT_PASS);
        Serial.print("Joining hotspot \"" TUNER_HOTSPOT_SSID "\"");
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000)
        {
            delay(250);
            Serial.print('.');
        }
        Serial.println();
        joined = (WiFi.status() == WL_CONNECTED);
        if (joined)
        {
            WiFi.setSleep(false);             // no modem sleep: quick replies to the page and the logger
            WiFi.setAutoReconnect(true);
        }
        else
        {
            Serial.println("Hotspot not found: making " TUNER_SSID " instead.");
            WiFi.disconnect(true);
        }
    }
    if (!joined)
    {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(TUNER_SSID, TUNER_PASSWORD);
    }
    WiFi.setTxPower(WIFI_POWER_11dBm);        // plenty for a few metres, less current

    if (MDNS.begin(TUNER_HOSTNAME)) MDNS.addService("http", "tcp", 80);

    tbServer.on("/", tbHttpPage);
    tbServer.on("/api/params", tbHttpParams);
    tbServer.on("/api/set", tbHttpSet);
    tbServer.on("/api/state", tbHttpState);
    tbServer.on("/api/result", tbHttpResult);
    tbServer.on("/api/run", tbHttpRun);
    tbServer.on("/api/stop", tbHttpStop);
    tbServer.on("/api/cmd", tbHttpCmd);
    tbServer.on("/api/export", tbHttpExport);
    tbServer.on("/api/trace", tbHttpTrace);
    tbServer.on("/api/maze", tbHttpMaze);
    tbServer.onNotFound(tbHttpNotFound);
    tbServer.begin();

    xTaskCreatePinnedToCore(tbWebTask, "tuner-web", 10240, NULL, 1, NULL, 0);

    if (joined)
    {
        Serial.print("Tuner on hotspot \"" TUNER_HOTSPOT_SSID "\": open http://" TUNER_HOSTNAME ".local or http://");
        Serial.println(WiFi.localIP().toString());
    }
    else
    {
        Serial.print("Tuner WiFi: join \"" TUNER_SSID "\" (password " TUNER_PASSWORD "), open http://");
        Serial.println(WiFi.softAPIP().toString());
    }
}

void setup()
{
    tbInitCore();

    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("ROBOFEST micromouse - TUNER build (WiFi ON: not for competition runs)");

    motors_begin();

    Serial.println("Keep the mouse completely still: starting ToFs and calibrating MPU6050...");
    tbSensorsOk = sensors_begin();
    if (tbSensorsOk)
    {
        Serial.print("MPU6050 Z-gyro bias (deg/s): ");
        Serial.println(sensors_gyroBiasDegPerSec(), 4);
    }
    else
    {
        Serial.println("ERROR: ToF/MPU6050 initialisation failed. Motor checks still work.");
    }

    tbLoadedFromFlash = tbNvsLoad();
    Serial.println(tbLoadedFromFlash ? "Loaded the values saved from the tuner."
                                     : "Using the TUNED VALUES block (nothing saved, or the block changed).");

    tbStatus("ready");
    tbStartWifi();
}

void loop()
{
    tbService();
}

#endif // TUNER_WIFI
