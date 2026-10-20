#ifndef PINS_H
#define PINS_H

// ============================================
// MICROMOUSE PIN DEFINITIONS - ESP32 Dev 30-Pin
// All pins chosen to avoid conflicts with WiFi/BT
// (no flash pins 6-11, no strapping pins 0/2/12/15, no UART0 1/3)
// ============================================

// ---------- I2C Bus (MPU6050 + all TOF sensors) ----------
#define PIN_SDA         21
#define PIN_SCL         22
#define I2C_CLOCK_SPEED 400000   // 400kHz Fast Mode

// ---------- TOF Sensor XSHUT Pins ----------
// Five-sensor build: 1 front, 2 left, 2 right. TOF6 is spare.
// Matches the real wiring (was 13/14/27/26/25, which swapped every sensor).
#define PIN_TOF1_XSHUT  26   // front-centre
#define PIN_TOF2_XSHUT  27   // left-front
#define PIN_TOF3_XSHUT  14   // left-rear
#define PIN_TOF4_XSHUT  25   // right-front
#define PIN_TOF5_XSHUT  13   // right-rear
#define PIN_TOF6_XSHUT  33   // spare (not fitted): sensors_begin() drives it LOW once,
                             // then the race build uses GPIO33 as the SPEED button

// Example I2C addresses to assign after XSHUT sequencing
#define TOF1_ADDR  0x30
#define TOF2_ADDR  0x31
#define TOF3_ADDR  0x32
#define TOF4_ADDR  0x33
#define TOF5_ADDR  0x34
#define TOF6_ADDR  0x35

// ---------- TB6612FNG Motor Driver ----------
// Firmware "A" = LEFT wheel, "B" = RIGHT wheel. On this robot the LEFT wheel
// is wired to the driver's B channel, so the A/B pin sets are swapped here.
#define PIN_PWMA    17   // LEFT motor speed (PWM)       - driver PWMB
#define PIN_AIN1    23   // LEFT motor direction 1       - driver BIN1 (moved from GPIO5 - boot safety)
#define PIN_AIN2    18   // LEFT motor direction 2       - driver BIN2
#define PIN_PWMB    32   // RIGHT motor speed (PWM)      - driver PWMA
#define PIN_BIN1    4    // RIGHT motor direction 1      - driver AIN1
#define PIN_BIN2    16   // RIGHT motor direction 2      - driver AIN2
// Driver STBY is wired to 3.3 V on the board (always enabled): no GPIO drives it.

// ---------- Race build start menu: buttons + LED ----------
// Each button between the GPIO and GND, internal pull-up, pressed = LOW.
#define PIN_BTN_MODE    19   // SEARCH <-> FAST
#define PIN_BTN_SPEED   33   // straight PWM +10; hold 2 s: back to the tuned value
#define PIN_BTN_BOOT     0   // the board's BOOT button: hold 5 s to erase the saved maze
#define PIN_LED          2   // the board's blue LED

// ---------- Encoders (quadrature, interrupt-capable) ----------
// Note: GPIO34/35/36/39 are input-only, ADC1 channels (safe w/ WiFi)
// Swapped the same way as the motors: "A" = LEFT wheel encoder.
#define PIN_ENCA_A  36   // LEFT encoder channel A (VP)
#define PIN_ENCA_B  39   // LEFT encoder channel B (VN)
#define PIN_ENCB_A  34   // RIGHT encoder channel A
#define PIN_ENCB_B  35   // RIGHT encoder channel B

#endif // PINS_H
