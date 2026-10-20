#include "sensors.h"
#include "pins.h"

#include <Wire.h>
#include <VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

static VL53L0X tofFront;
static VL53L0X tofLeftFront;
static VL53L0X tofLeftRear;
static VL53L0X tofRightFront;
static VL53L0X tofRightRear;
static Adafruit_MPU6050 mpu;

static float gyroZBiasDegPerSec = 0.0f;
static float integratedYawDeg = 0.0f;
static uint32_t lastGyroMicros = 0;

static bool initOneToF(VL53L0X &sensor, int xshutPin, uint8_t address)
{
    digitalWrite(xshutPin, HIGH);
    delay(10);

    sensor.setTimeout(60);
    if (!sensor.init())
    {
        return false;
    }

    sensor.setAddress(address);
    sensor.setMeasurementTimingBudget(20000);
    sensor.startContinuous(20);
    return true;
}

// Calibration and median filtering are applied in tuner_bridge.h /
// tb_tofProcess() (mouse-firmware.ino). 8190 = timeout / nothing in range.
static uint16_t readToF(VL53L0X &sensor)
{
    uint16_t value = sensor.readRangeContinuousMillimeters();

    if (sensor.timeoutOccurred() || value == 0 || value >= 8190)
    {
        return 8190;
    }

    return value;
}

static bool validToF(uint16_t value)
{
    return value > 0 && value < TOF_VALID_MAX_MM;
}

static bool sidePairSaysWall(uint16_t frontValue, uint16_t rearValue)
{
    bool frontValid = validToF(frontValue);
    bool rearValid = validToF(rearValue);

    bool frontWall = frontValid && frontValue < SIDE_WALL_THRESHOLD_MM;
    bool rearWall = rearValid && rearValue < SIDE_WALL_THRESHOLD_MM;

    if (frontWall && rearWall)
    {
        return true;
    }

    // Conservative anti-graze rule: one very close side sensor is enough to
    // declare a wall, even if its pair has a temporary bad reading.
    if (frontValid && frontValue < SIDE_STRONG_WALL_THRESHOLD_MM)
    {
        return true;
    }

    if (rearValid && rearValue < SIDE_STRONG_WALL_THRESHOLD_MM)
    {
        return true;
    }

    if (frontValid && !rearValid)
    {
        return frontWall;
    }

    if (rearValid && !frontValid)
    {
        return rearWall;
    }

    return false;
}

static float wallDistanceFromPair(uint16_t frontValue, uint16_t rearValue)
{
    bool frontUsable = validToF(frontValue) && frontValue < SIDE_WALL_THRESHOLD_MM;
    bool rearUsable = validToF(rearValue) && rearValue < SIDE_WALL_THRESHOLD_MM;

    if (frontUsable && rearUsable)
    {
        return ((float)frontValue + (float)rearValue) * 0.5f;
    }

    if (frontUsable) return (float)frontValue;
    if (rearUsable) return (float)rearValue;

    return TARGET_SIDE_DISTANCE_MM;
}

static float leftAngleErrorDeg(uint16_t frontValue, uint16_t rearValue)
{
    if (!(validToF(frontValue) && validToF(rearValue)))
    {
        return 0.0f;
    }

    if (frontValue >= SIDE_WALL_THRESHOLD_MM || rearValue >= SIDE_WALL_THRESHOLD_MM)
    {
        return 0.0f;
    }

    // Nose toward left wall => left-front < left-rear => negative => steer right.
    float differenceMm = (float)frontValue - (float)rearValue;
    return atan2f(differenceMm, SIDE_SENSOR_PAIR_SPACING_MM) * 180.0f / PI;
}

static float rightAngleErrorDeg(uint16_t frontValue, uint16_t rearValue)
{
    if (!(validToF(frontValue) && validToF(rearValue)))
    {
        return 0.0f;
    }

    if (frontValue >= SIDE_WALL_THRESHOLD_MM || rearValue >= SIDE_WALL_THRESHOLD_MM)
    {
        return 0.0f;
    }

    // Nose toward right wall => right-front < right-rear => positive => steer left.
    float differenceMm = (float)rearValue - (float)frontValue;
    return atan2f(differenceMm, SIDE_SENSOR_PAIR_SPACING_MM) * 180.0f / PI;
}

static float rawGyroZDegPerSec()
{
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;

    mpu.getEvent(&accel, &gyro, &temp);
    float z = gyro.gyro.z * 180.0f / PI;

#if GYRO_Z_INVERTED
    z = -z;
#endif

    return z;
}

static void calibrateGyroBias()
{
    float sum = 0.0f;

    // Robot MUST stay completely still here.
    delay(400);

    for (int i = 0; i < GYRO_CALIBRATION_SAMPLES; i++)
    {
        sum += rawGyroZDegPerSec();
        delay(GYRO_CALIBRATION_DELAY_MS);
    }

    gyroZBiasDegPerSec = sum / (float)GYRO_CALIBRATION_SAMPLES;
}

bool sensors_begin()
{
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(I2C_CLOCK_SPEED);

    pinMode(PIN_TOF1_XSHUT, OUTPUT);
    pinMode(PIN_TOF2_XSHUT, OUTPUT);
    pinMode(PIN_TOF3_XSHUT, OUTPUT);
    pinMode(PIN_TOF4_XSHUT, OUTPUT);
    pinMode(PIN_TOF5_XSHUT, OUTPUT);
    pinMode(PIN_TOF6_XSHUT, OUTPUT);

    digitalWrite(PIN_TOF1_XSHUT, LOW);
    digitalWrite(PIN_TOF2_XSHUT, LOW);
    digitalWrite(PIN_TOF3_XSHUT, LOW);
    digitalWrite(PIN_TOF4_XSHUT, LOW);
    digitalWrite(PIN_TOF5_XSHUT, LOW);
    digitalWrite(PIN_TOF6_XSHUT, LOW);
    delay(25);

    if (!initOneToF(tofFront,      PIN_TOF1_XSHUT, TOF1_ADDR)) return false;
    if (!initOneToF(tofLeftFront,  PIN_TOF2_XSHUT, TOF2_ADDR)) return false;
    if (!initOneToF(tofLeftRear,   PIN_TOF3_XSHUT, TOF3_ADDR)) return false;
    if (!initOneToF(tofRightFront, PIN_TOF4_XSHUT, TOF4_ADDR)) return false;
    if (!initOneToF(tofRightRear,  PIN_TOF5_XSHUT, TOF5_ADDR)) return false;

    // TOF6 remains shut down as a spare.

    if (!mpu.begin(MPU6050_ADDRESS, &Wire))
    {
        return false;
    }

    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);

    calibrateGyroBias();
    sensors_resetYaw();

    return true;
}

uint16_t sensors_frontDistanceMm()
{
    return readToF(tofFront);
}

uint16_t sensors_leftFrontDistanceMm()
{
    return readToF(tofLeftFront);
}

uint16_t sensors_leftRearDistanceMm()
{
    return readToF(tofLeftRear);
}

uint16_t sensors_rightFrontDistanceMm()
{
    return readToF(tofRightFront);
}

uint16_t sensors_rightRearDistanceMm()
{
    return readToF(tofRightRear);
}

bool sensors_wallFront()
{
    uint16_t value = sensors_frontDistanceMm();
    return validToF(value) && value < FRONT_WALL_THRESHOLD_MM;
}

bool sensors_wallLeft()
{
    uint16_t frontValue = sensors_leftFrontDistanceMm();
    uint16_t rearValue = sensors_leftRearDistanceMm();
    return sidePairSaysWall(frontValue, rearValue);
}

bool sensors_wallRight()
{
    uint16_t frontValue = sensors_rightFrontDistanceMm();
    uint16_t rearValue = sensors_rightRearDistanceMm();
    return sidePairSaysWall(frontValue, rearValue);
}

void sensors_readSideGuidance(SideGuidance *guidance)
{
    if (guidance == nullptr)
    {
        return;
    }

    uint16_t lf = sensors_leftFrontDistanceMm();
    uint16_t lr = sensors_leftRearDistanceMm();
    uint16_t rf = sensors_rightFrontDistanceMm();
    uint16_t rr = sensors_rightRearDistanceMm();

    guidance->leftWall = sidePairSaysWall(lf, lr);
    guidance->rightWall = sidePairSaysWall(rf, rr);

    guidance->leftMinMm = 8190;
    guidance->rightMinMm = 8190;

    if (validToF(lf)) guidance->leftMinMm = lf;
    if (validToF(lr) && lr < guidance->leftMinMm) guidance->leftMinMm = lr;
    if (validToF(rf)) guidance->rightMinMm = rf;
    if (validToF(rr) && rr < guidance->rightMinMm) guidance->rightMinMm = rr;

    float leftDistance = wallDistanceFromPair(lf, lr);
    float rightDistance = wallDistanceFromPair(rf, rr);

    if (guidance->leftWall && guidance->rightWall)
    {
        guidance->lateralErrorMm = leftDistance - rightDistance;
    }
    else if (guidance->leftWall)
    {
        guidance->lateralErrorMm = leftDistance - TARGET_SIDE_DISTANCE_MM;
    }
    else if (guidance->rightWall)
    {
        guidance->lateralErrorMm = TARGET_SIDE_DISTANCE_MM - rightDistance;
    }
    else
    {
        guidance->lateralErrorMm = 0.0f;
    }

    float leftAngle = guidance->leftWall ? leftAngleErrorDeg(lf, lr) : 0.0f;
    float rightAngle = guidance->rightWall ? rightAngleErrorDeg(rf, rr) : 0.0f;

    if (guidance->leftWall && guidance->rightWall)
    {
        guidance->angleErrorDeg = (leftAngle + rightAngle) * 0.5f;
    }
    else if (guidance->leftWall)
    {
        guidance->angleErrorDeg = leftAngle;
    }
    else if (guidance->rightWall)
    {
        guidance->angleErrorDeg = rightAngle;
    }
    else
    {
        guidance->angleErrorDeg = 0.0f;
    }
}

void sensors_resetYaw()
{
    integratedYawDeg = 0.0f;
    lastGyroMicros = micros();
}

float sensors_gyroZDegPerSec()
{
    return rawGyroZDegPerSec() - gyroZBiasDegPerSec;
}

float sensors_yawDegrees()
{
    uint32_t now = micros();
    uint32_t elapsed = now - lastGyroMicros;
    lastGyroMicros = now;

    float dt = elapsed / 1000000.0f;

    // Avoid a giant integration jump after blocking setup/debug delays.
    if (dt <= 0.0f || dt > 0.1f)
    {
        dt = 0.0f;
    }

    integratedYawDeg += sensors_gyroZDegPerSec() * dt;
    return integratedYawDeg;
}

float sensors_gyroBiasDegPerSec()
{
    return gyroZBiasDegPerSec;
}