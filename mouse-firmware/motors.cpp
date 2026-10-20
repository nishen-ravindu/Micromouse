#include "motors.h"
#include "pins.h"
#include "sensors.h"

#include <math.h>
#include <stdlib.h>

static volatile long encoderA = 0;
static volatile long encoderB = 0;

static int clampPwm(int value)
{
    if (value > 255) return 255;
    if (value < -255) return -255;
    return value;
}

static void IRAM_ATTR encoderAIsr()
{
    bool a = digitalRead(PIN_ENCA_A);
    bool b = digitalRead(PIN_ENCA_B);
    long delta = (a == b) ? 1 : -1;

#if ENCODER_A_INVERTED
    delta = -delta;
#endif

    encoderA += delta;
}

static void IRAM_ATTR encoderBIsr()
{
    bool a = digitalRead(PIN_ENCB_A);
    bool b = digitalRead(PIN_ENCB_B);
    long delta = (a == b) ? 1 : -1;

#if ENCODER_B_INVERTED
    delta = -delta;
#endif

    encoderB += delta;
}

static void driveOneMotor(
    int command,
    int in1,
    int in2,
    int pwmPin,
    bool inverted)
{
    command = clampPwm(command);

    if (inverted)
    {
        command = -command;
    }

    if (command > 0)
    {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
        analogWrite(pwmPin, command);
    }
    else if (command < 0)
    {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
        analogWrite(pwmPin, -command);
    }
    else
    {
        analogWrite(pwmPin, 0);
        digitalWrite(in1, LOW);
        digitalWrite(in2, LOW);
    }
}

static void setWheelSpeeds(int left, int right)
{
    // Motor A = left, Motor B = right (motors.h).
    driveOneMotor(left, PIN_AIN1, PIN_AIN2, PIN_PWMA, MOTOR_A_INVERTED);
    driveOneMotor(right, PIN_BIN1, PIN_BIN2, PIN_PWMB, MOTOR_B_INVERTED);
}

static void brakeOneMotor(int in1, int in2, int pwmPin)
{
    digitalWrite(in1, HIGH);
    digitalWrite(in2, HIGH);
    analogWrite(pwmPin, 255);
}

static void brakeBoth()
{
    brakeOneMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA);
    brakeOneMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB);
    delay(45);
    motors_stop();
}

bool motors_begin()
{
    pinMode(PIN_AIN1, OUTPUT);
    pinMode(PIN_AIN2, OUTPUT);
    pinMode(PIN_PWMA, OUTPUT);

    pinMode(PIN_BIN1, OUTPUT);
    pinMode(PIN_BIN2, OUTPUT);
    pinMode(PIN_PWMB, OUTPUT);

    pinMode(PIN_STBY, OUTPUT);
    digitalWrite(PIN_STBY, HIGH);

    // GPIO34/35/36/39 have no internal pull-ups on ESP32.
    pinMode(PIN_ENCA_A, INPUT);
    pinMode(PIN_ENCA_B, INPUT);
    pinMode(PIN_ENCB_A, INPUT);
    pinMode(PIN_ENCB_B, INPUT);

    attachInterrupt(digitalPinToInterrupt(PIN_ENCA_A), encoderAIsr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCB_A), encoderBIsr, CHANGE);

    motors_stop();
    return true;
}

void motors_stop()
{
    setWheelSpeeds(0, 0);
}

long motors_leftEncoder()
{
    noInterrupts();
    long value = encoderA;
    interrupts();
    return value;
}

long motors_rightEncoder()
{
    noInterrupts();
    long value = encoderB;
    interrupts();
    return value;
}

void motors_resetEncoders()
{
    noInterrupts();
    encoderA = 0;
    encoderB = 0;
    interrupts();
}

static float countsToMm(long counts)
{
    const float circumference = PI * WHEEL_DIAMETER_MM;
    return ((float)counts / ENCODER_COUNTS_PER_REV) * circumference;
}

// Drives `cells` cells in one go: the slow-down ramp and the brake happen only
// at the end. motors_moveOneCell() is this with cells = 1.
static bool moveCells(int cells)
{
    motors_resetEncoders();

    // MPU6050 yaw is zeroed at the beginning of every cell. This keeps gyro
    // drift bounded while still giving strong short-term heading correction.
    sensors_resetYaw();

    uint32_t started = millis();
    uint32_t lastSensorSample = 0;

    float sideLateralError = 0.0f;
    float sideAngleError = 0.0f;
    uint16_t leftClearance = 8190;
    uint16_t rightClearance = 8190;
    uint16_t frontClearance = 8190;

    while (true)
    {
        long leftCounts = labs(motors_leftEncoder());
        long rightCounts = labs(motors_rightEncoder());

        float leftMm = countsToMm(leftCounts);
        float rightMm = countsToMm(rightCounts);
        float averageMm = (leftMm + rightMm) * 0.5f;

        if (averageMm >= CELL_DISTANCE_MM * cells)
        {
            brakeBoth();
            return true;
        }

        if (millis() - started > 3800UL * (uint32_t)cells)
        {
            motors_stop();
            return false;
        }

        // Side ToFs do the slow geometric correction; MPU6050 runs every loop.
        if (lastSensorSample == 0 || millis() - lastSensorSample >= 30)
        {
            lastSensorSample = millis();

            frontClearance = sensors_frontDistanceMm();

            SideGuidance guidance;
            sensors_readSideGuidance(&guidance);

            sideLateralError = guidance.lateralErrorMm;
            sideAngleError = guidance.angleErrorDeg;
            leftClearance = guidance.leftMinMm;
            rightClearance = guidance.rightMinMm;
        }

        // If the unexpected physical world disagrees with the map, protect the
        // robot first and let floodfill replan after the move reports failure.
        if (averageMm > 18.0f && frontClearance < FRONT_EMERGENCY_MM)
        {
            motors_stop();
            return false;
        }

        if (leftClearance < SIDE_HARD_STOP_MM ||
            rightClearance < SIDE_HARD_STOP_MM)
        {
            motors_stop();
            return false;
        }

        float remaining = CELL_DISTANCE_MM * cells - averageMm;
        int basePwm = STRAIGHT_PWM;

        if (remaining < 48.0f)
        {
            float fraction = remaining / 48.0f;
            if (fraction < 0.0f) fraction = 0.0f;
            if (fraction > 1.0f) fraction = 1.0f;

            basePwm = STRAIGHT_MIN_PWM +
                (int)((STRAIGHT_PWM - STRAIGHT_MIN_PWM) * fraction);
        }

        float encoderError = (float)(leftCounts - rightCounts);

        // MPU6050 heading error: positive yaw means nose rotated left from the
        // commanded straight heading. Positive steer below also turns left, so
        // we SUBTRACT yaw to command a correcting right steer.
        float yawError = sensors_yawDegrees();

        // Positive steer = turn LEFT (slow left wheel, speed right wheel).
        float steer =
            KP_ENCODER_SYNC * encoderError
            - KP_GYRO_HEADING * yawError
            + KP_SIDE_LATERAL * sideLateralError
            + KP_SIDE_ANGLE * sideAngleError;

        // Anti-graze bias begins well before the hard stop.
        if (leftClearance < SIDE_DANGER_MM)
        {
            steer -= KP_SIDE_DANGER * (SIDE_DANGER_MM - leftClearance);
        }

        if (rightClearance < SIDE_DANGER_MM)
        {
            steer += KP_SIDE_DANGER * (SIDE_DANGER_MM - rightClearance);
        }

        int leftCommand = (int)(basePwm - steer);
        int rightCommand = (int)(basePwm + steer);

        if (leftCommand < STRAIGHT_MIN_PWM) leftCommand = STRAIGHT_MIN_PWM;
        if (rightCommand < STRAIGHT_MIN_PWM) rightCommand = STRAIGHT_MIN_PWM;
        if (leftCommand > 255) leftCommand = 255;
        if (rightCommand > 255) rightCommand = 255;

        setWheelSpeeds(leftCommand, rightCommand);
        delay(2);
    }
}

bool motors_moveOneCell()
{
    return moveCells(1);
}

bool motors_moveCells(int cells)
{
    if (cells < 1) cells = 1;
    return moveCells(cells);
}

static bool turn90(bool leftTurn)
{
    sensors_resetYaw();
    motors_resetEncoders();

    uint32_t started = millis();

    while (true)
    {
        float yaw = sensors_yawDegrees();

        // With normal MPU6050 orientation, CCW/left is positive and
        // CW/right is negative. GYRO_Z_INVERTED in sensors.h corrects a flipped
        // mounting orientation. Using signed progress catches wrong motor signs.
        float progress = leftTurn ? yaw : -yaw;

        if (progress >= TURN_STOP_DEG)
        {
            brakeBoth();
            delay(35);
            return true;
        }

        // If the measured yaw moves strongly in the wrong direction, stop
        // instead of spinning blindly until timeout.
        if (progress < -12.0f && millis() - started > 180)
        {
            motors_stop();
            return false;
        }

        if (millis() - started > TURN_TIMEOUT_MS)
        {
            motors_stop();
            return false;
        }

        int pwm = progress < TURN_SLOW_START_DEG ? TURN_PWM : TURN_SLOW_PWM;

        if (leftTurn)
        {
            setWheelSpeeds(-pwm, pwm);
        }
        else
        {
            setWheelSpeeds(pwm, -pwm);
        }

        delay(2);
    }
}

bool motors_turnLeft90()
{
    return turn90(true);
}

bool motors_turnRight90()
{
    return turn90(false);
}