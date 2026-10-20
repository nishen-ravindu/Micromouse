#ifndef MOTORS_H
#define MOTORS_H

#include <Arduino.h>

// ---------------- Mechanical geometry ----------------
#define MOUSE_OVERALL_WIDTH_MM      80.0f
#define MOUSE_OVERALL_LENGTH_MM    120.0f
#define WHEEL_DIAMETER_MM           32.0f
#define WHEEL_TRACK_MM              72.0f
#define CELL_DISTANCE_MM           180.0f

// IMPORTANT: placeholder until measured on your exact motor/encoder/gearbox.
#define ENCODER_COUNTS_PER_REV     660.0f

// Motor A = left, Motor B = right in this firmware.
// (pins.h maps A/B so that A really is the left wheel on this robot.)
#define MOTOR_A_INVERTED              0
#define MOTOR_B_INVERTED              1
#define ENCODER_A_INVERTED            1
#define ENCODER_B_INVERTED            0

// Conservative starting speeds for first physical tests.
#define STRAIGHT_PWM                 120
#define STRAIGHT_MIN_PWM              72
#define TURN_PWM                     100
#define TURN_SLOW_PWM                 68

// Turn profile using MPU6050 yaw.
#define TURN_SLOW_START_DEG          67.0f
#define TURN_STOP_DEG                88.5f
#define TURN_TIMEOUT_MS              2400

// Straight-line controller gains.
// Steering fuses encoder sync + MPU6050 heading + 5-ToF wall geometry.
#define KP_ENCODER_SYNC              0.18f
#define KP_GYRO_HEADING              1.45f
#define KP_SIDE_LATERAL              0.72f
#define KP_SIDE_ANGLE                1.75f
#define KP_SIDE_DANGER               2.80f

bool motors_begin();
void motors_stop();

bool motors_moveOneCell();
bool motors_moveCells(int cells);    // several cells in one go, brake only at the end
bool motors_turnLeft90();
bool motors_turnRight90();

long motors_leftEncoder();
long motors_rightEncoder();
void motors_resetEncoders();

#endif
