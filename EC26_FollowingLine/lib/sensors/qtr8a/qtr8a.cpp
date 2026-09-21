#include "qtr8a.h"
#include <Arduino.h>

QTR8A qtr;

void QTR8A::begin() {
    _qtr.setTypeAnalog();
    _qtr.setSensorPins((const uint8_t *)qtrPins, QTR_SENSOR_COUNT);
    _qtr.setEmitterPin(QTR_EMITTER_PIN);
    clearCalibration();
    readRawSensors();
    updatePreviewValues();
}

void QTR8A::clearCalibration() {
    for (int i = 0; i < QTR_SENSOR_COUNT; i++) {
        _rawValues[i] = 0;
        _sensorValues[i] = 0;
        _whiteValues[i] = 0;
        _blackValues[i] = 0;
        _thresholdValues[i] = 0;
    }

    _hasWhiteCalibration = false;
    _hasBlackCalibration = false;
    _isCalibrated = false;
    _onLine = false;
    _lastPosition = QTR_LINE_CENTER;
    _lastValidPosition = QTR_LINE_CENTER;
}

void QTR8A::readRawSensors() {
    _qtr.read(_rawValues);
}

void QTR8A::updateThresholds() {
    for (int i = 0; i < QTR_SENSOR_COUNT; i++) {
        int midpoint = ((int)_whiteValues[i] + (int)_blackValues[i]) / 2;
        int threshold = midpoint;

        if (_blackValues[i] <= _whiteValues[i]) {
            threshold += QTR_THRESHOLD_OFFSET_RAW;
        } else {
            threshold -= QTR_THRESHOLD_OFFSET_RAW;
        }

        _thresholdValues[i] = (uint16_t)constrain(threshold, 0, QTR_RAW_MAX);
    }
}

uint16_t QTR8A::normalizeSensor(uint8_t index, uint16_t rawValue) const {
    if (!_isCalibrated || index >= QTR_SENSOR_COUNT) {
        return 0;
    }

    int white = (int)_whiteValues[index];
    int black = (int)_blackValues[index];
    int range = abs(white - black);

    if (range < 5) {
        return 0;
    }

    long normalized = 0;
    if (black < white) {
        normalized = ((long)white - (long)rawValue) * 1000L / range;
    } else {
        normalized = ((long)rawValue - (long)white) * 1000L / range;
    }

    normalized = constrain(normalized, 0L, 1000L);
    return (uint16_t)normalized;
}

bool QTR8A::isBlackDetected(uint8_t index, uint16_t rawValue) const {
    if (!_isCalibrated || index >= QTR_SENSOR_COUNT) {
        return false;
    }

    if (_blackValues[index] <= _whiteValues[index]) {
        return rawValue <= _thresholdValues[index];
    }

    return rawValue >= _thresholdValues[index];
}

void QTR8A::updatePreviewValues() {
    for (int i = 0; i < QTR_SENSOR_COUNT; i++) {
        _sensorValues[i] = (uint16_t)map(_rawValues[i], 0, QTR_RAW_MAX, 1000, 0);
    }
}

bool QTR8A::calibrateWhite() {
    uint32_t sums[QTR_SENSOR_COUNT] = {0};

    Serial.println("[QTR] White calibration started...");
    for (int sample = 0; sample < QTR_CALIBRATION_SAMPLES; sample++) {
        readRawSensors();
        for (int i = 0; i < QTR_SENSOR_COUNT; i++) {
            sums[i] += _rawValues[i];
        }
        delay(5);
        yield();
    }

    for (int i = 0; i < QTR_SENSOR_COUNT; i++) {
        _whiteValues[i] = (uint16_t)(sums[i] / QTR_CALIBRATION_SAMPLES);
    }

    _hasWhiteCalibration = true;
    _isCalibrated = _hasWhiteCalibration && _hasBlackCalibration;
    if (_isCalibrated) {
        updateThresholds();
    }
    readRawSensors();
    updatePreviewValues();
    Serial.println("[QTR] White calibration complete.");
    return true;
}

bool QTR8A::calibrateBlack() {
    uint32_t sums[QTR_SENSOR_COUNT] = {0};

    Serial.println("[QTR] Black calibration started...");
    for (int sample = 0; sample < QTR_CALIBRATION_SAMPLES; sample++) {
        readRawSensors();
        for (int i = 0; i < QTR_SENSOR_COUNT; i++) {
            sums[i] += _rawValues[i];
        }
        delay(5);
        yield();
    }

    for (int i = 0; i < QTR_SENSOR_COUNT; i++) {
        _blackValues[i] = (uint16_t)(sums[i] / QTR_CALIBRATION_SAMPLES);
    }

    _hasBlackCalibration = true;
    _isCalibrated = _hasWhiteCalibration && _hasBlackCalibration;

    if (_isCalibrated) {
        updateThresholds();
    }

    readRawSensors();
    updatePreviewValues();
    Serial.println("[QTR] Black calibration complete.");
    return true;
}

float QTR8A::readLine() {
    readRawSensors();

    if (!_isCalibrated) {
        updatePreviewValues();
        _onLine = false;
        _lastPosition = _lastValidPosition;
        return _lastPosition;
    }

    long weightedSum = 0;
    uint8_t activeCount = 0;

    for (int i = 0; i < QTR_SENSOR_COUNT; i++) {
        _sensorValues[i] = normalizeSensor(i, _rawValues[i]);
        if (isBlackDetected(i, _rawValues[i])) {
            weightedSum += (long)i * QTR_POSITION_STEP;
            activeCount++;
        }
    }

    _onLine = (activeCount > 0);

    if (_onLine) {
        _lastValidPosition = (float)weightedSum / activeCount;
        _lastPosition = _lastValidPosition;
    } else {
        _lastPosition = _lastValidPosition;
    }

    return _lastPosition;
}

float QTR8A::getError() {
    return readLine() - QTR_LINE_CENTER;
}

uint16_t QTR8A::getSensor(uint8_t index) const {
    if (index >= QTR_SENSOR_COUNT) return 0;
    return _sensorValues[index];
}
