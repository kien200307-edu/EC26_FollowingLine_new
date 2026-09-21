#pragma once

#include <QTRSensors.h>
#include "core/config.h"

class QTR8A {
public:
    void  begin();
    float readLine();       // Returns line position in the range 0..700
    float getError();       // Returns signed error relative to QTR_LINE_CENTER
    bool  calibrateWhite(); // Runs blocking white-surface calibration
    bool  calibrateBlack(); // Runs blocking black-line calibration
    void  clearCalibration();
    bool  isCalibrated() const { return _isCalibrated; }

    bool isOnLine() const { return _onLine; }
    uint16_t getSensor(uint8_t index) const;
    const uint16_t* getSensorValues() const { return _sensorValues; }

private:
    void readRawSensors();
    void updateThresholds();
    void updatePreviewValues();
    uint16_t normalizeSensor(uint8_t index, uint16_t rawValue) const;
    bool isBlackDetected(uint8_t index, uint16_t rawValue) const;

    QTRSensors _qtr;
    uint16_t   _rawValues[QTR_SENSOR_COUNT] = {0};
    uint16_t   _sensorValues[QTR_SENSOR_COUNT] = {0};
    uint16_t   _whiteValues[QTR_SENSOR_COUNT] = {0};
    uint16_t   _blackValues[QTR_SENSOR_COUNT] = {0};
    uint16_t   _thresholdValues[QTR_SENSOR_COUNT] = {0};
    bool       _hasWhiteCalibration = false;
    bool       _hasBlackCalibration = false;
    bool       _isCalibrated = false;
    bool       _onLine = false;
    float      _lastPosition = QTR_LINE_CENTER;
    float      _lastValidPosition = QTR_LINE_CENTER;
};

extern QTR8A qtr;
