#pragma once
#include <Arduino.h>
// =====================================================
// MOTOR L298N
// =====================================================

// LEFT
#define MOTOR_ENA 27
#define MOTOR_IN1 14
#define MOTOR_IN2 13

// RIGHT
#define MOTOR_ENB 22
#define MOTOR_IN3 21
#define MOTOR_IN4 19

// =====================================================
// PWM
// =====================================================

#define PWM_FREQ       1000
#define PWM_RESOLUTION 8

#define PWM_CHANNEL_LEFT  0
#define PWM_CHANNEL_RIGHT 1

// =====================================================
// QTR8A
// =====================================================

#define QTR_SENSOR_COUNT 8
#define QTR_ACTIVE_THRESHOLD 400
#define QTR_POSITION_STEP 100
#define QTR_LINE_CENTER (((QTR_SENSOR_COUNT - 1) * QTR_POSITION_STEP) / 2)
#define QTR_CALIBRATION_SAMPLES 150
#define QTR_RAW_MAX 4095
#define QTR_THRESHOLD_OFFSET_RAW 200

static const uint8_t qtrPins[QTR_SENSOR_COUNT] = {
    26, 25, 33, 32, 35, 34, 39, 36
};

#define QTR_EMITTER_PIN 2


// Hệ số khuếch đại cho từng sensor (bù đắp sensor yếu)
static const float SENSOR_GAINS[QTR_SENSOR_COUNT] = {
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
};

// =====================================================
// SERVO
// =====================================================

#define SERVO_LEFT_PIN  18
#define SERVO_RIGHT_PIN 17

// =====================================================
// HC-SR04
// =====================================================

#define ULTRASONIC_TRIG 5
#define ULTRASONIC_ECHO 23

// =====================================================
// IR SINGLE
// =====================================================

#define IR_SINGLE_PIN 4

// =====================================================
// LED INDICATOR
// Sáng = Auto Mode | Tắt = Manual Mode
// =====================================================

#define LED_MODE_PIN 2   // LED_BUILTIN trên ESP32 thường là GPIO2

// =====================================================
// NÚT BẤM CHUYỂN CHẾ ĐỘ
// =====================================================

#define BTN_MODE_PIN 16
#define BTN_AUTO3_PIN 15


// =====================================================
// ROBOT SPEEDS
// =====================================================

#define MANUAL_SPEED_DEFAULT 150   // Tốc độ mặc định Manual (0–255)

#define PID_BASE_SPEED_DEFAULT 150 // Tốc độ nền PID mặc định



// =====================================================
// PID DEFAULTS
// =====================================================

#define PID_KP_DEFAULT  1.2f
#define PID_KI_DEFAULT  0.02f
#define PID_KD_DEFAULT  4.0f


// PID cho góc 45 độ
#define PID45_KP_DEFAULT 0.5f
#define PID45_KI_DEFAULT 0.0f
#define PID45_KD_DEFAULT 1.2f

// Giới hạn chỉnh PID qua UART
#define PID_PARAM_MIN  0.0f
#define PID_PARAM_MAX 500.0f


// =====================================================
// BLE
// =====================================================

#define BLE_DEVICE_NAME "ESP32_Robot"
