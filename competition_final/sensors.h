#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// -----------------------------------------------------------------------------
// FIVE-SENSOR PHYSICAL LAYOUT
// -----------------------------------------------------------------------------
// Robot body: about 80 mm wide x 120 mm long.
// Wheel axle: close to geometric centre of robot.
//
// TOF1 = FRONT-CENTRE, on centreline, about +30 mm ahead of axle.
// TOF2 = LEFT-FRONT,  close to left body edge, about +25 mm ahead of axle.
// TOF3 = LEFT-REAR,   close to left body edge, about -25 mm behind axle.
// TOF4 = RIGHT-FRONT, close to right body edge, about +25 mm ahead of axle.
// TOF5 = RIGHT-REAR,  close to right body edge, about -25 mm behind axle.
//
// Side sensors point exactly 90 degrees sideways. Keep their optical axes well
// below the 50 mm maze-wall top; around 20-30 mm is preferred.
// -----------------------------------------------------------------------------

#define MOUSE_WIDTH_MM                    80.0f
#define MOUSE_LENGTH_MM                  120.0f
#define FRONT_SENSOR_FORWARD_MM           30.0f
#define SIDE_SENSOR_FORWARD_OFFSET_MM     25.0f
#define SIDE_SENSOR_LATERAL_MM            40.0f
#define SIDE_SENSOR_PAIR_SPACING_MM       50.0f
#define SENSOR_OPTICAL_HEIGHT_MM          25.0f

// 180 mm pitch, 12 mm wall thickness => ~168 mm open corridor.
// Half corridor = 84 mm. Side sensor face near +/-40 mm => ~44 mm centred.
#define TARGET_SIDE_DISTANCE_MM           44.0f

// Starting wall thresholds. Tune on the completed chassis.
#define FRONT_WALL_THRESHOLD_MM           90
#define SIDE_WALL_THRESHOLD_MM            95
#define SIDE_STRONG_WALL_THRESHOLD_MM     65

// Safety limits while moving. Raised slightly versus the previous build to
// give more clearance before any possible wall graze.
#define FRONT_EMERGENCY_MM                28
#define SIDE_DANGER_MM                    30
#define SIDE_HARD_STOP_MM                 18

#define TOF_VALID_MAX_MM                1200

// MPU6050 configuration.
#define MPU6050_ADDRESS                 0x68
#define GYRO_CALIBRATION_SAMPLES         600
#define GYRO_CALIBRATION_DELAY_MS          2

// Normal mounting assumes +Z points upward. If left turns report NEGATIVE yaw,
// change this to 1 instead of rewriting controller signs.
#define GYRO_Z_INVERTED                    0

struct SideGuidance
{
    bool leftWall;
    bool rightWall;

    // Positive means the motion controller should steer LEFT.
    float lateralErrorMm;
    float angleErrorDeg;

    uint16_t leftMinMm;
    uint16_t rightMinMm;
};

bool sensors_begin();

uint16_t sensors_frontDistanceMm();
uint16_t sensors_leftFrontDistanceMm();
uint16_t sensors_leftRearDistanceMm();
uint16_t sensors_rightFrontDistanceMm();
uint16_t sensors_rightRearDistanceMm();

bool sensors_wallFront();
bool sensors_wallLeft();
bool sensors_wallRight();

void sensors_readSideGuidance(SideGuidance *guidance);

// MPU6050 / gyro API used by motors.cpp.
void sensors_resetYaw();
float sensors_yawDegrees();
float sensors_gyroZDegPerSec();
float sensors_gyroBiasDegPerSec();

#include "tuner_bridge.h"   // live tuning + ToF calibration (see that file)

#endif
