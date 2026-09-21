#pragma once
#include <Arduino.h>
#include "utils/pid.h"
#include "junction_detector.h"
#include "mission_manager.h"

enum ControlMode {
  MODE_MANUAL,
  MODE_AUTOMATION
};

enum TurnMode {
  TURN_STRAIGHT,
  TURN_LIGHT,
  TURN_MEDIUM,
  TURN_SHARP
};

// =====================================================
// Robot Logic – điều phối toàn bộ hành vi robot
// =====================================================

class RobotLogic {
public:
    void begin();
    void update();
    void updateBLE(const String& cmd);
    void setControlMode(ControlMode mode);
    // Tuning parameters
    void setPidKp(float v);
    void setPidKi(float v);
    void setPidKd(float v);
    void setBaseSpeed(int v);
    void setTurnSpeed(int v);
    void setSharpTurnSpeed(int v);

    // Getters for sensor data
    const uint16_t* getSensorValues() const;
    float getDistance() const;
    bool getIRDetection() const;
    ControlMode getControlMode() const { return currentControlMode; }
    bool isConnected() const { return _connected; }
    
    // For sending telemetry
    void sendSensorData(bool force = false);
    void sendDebugLog(const String& msg);

private:
    void selectAutoMission(AutoMission mission);
    void startRobot();
    void stopRobot();
    void runWhiteCalibration();
    void runBlackCalibration();
    void sendRunState();
    void sendStatusSnapshot();
    void handleControlPad(char btn, bool pressed);
    void handleSlider(char num, float value);
    void handleUARTTuning(const String& cmd);
    void sampleLineTelemetry(long* weightedSum = nullptr, long* sum = nullptr);
    void followLinePID();
    void normalPID();
    void runCalibration();
    void turnLeft90();
    void turnRight90();
    void manualDrive();
    void autoGrabLogic();
    void rescueLogic();
    void updateLED();
    void updateControlModeButton();

    // Sensor arrays
    uint16_t _sensorRaw[8] = {0};
    uint8_t _sensorDigital[8] = {0};
    uint16_t _sensorMin[8] = {4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095};
    uint16_t _sensorMax[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    float _position = 3500;
    
    // Calibration state
    bool _isCalibrating = false;
    unsigned long _calibStartTime = 0;
    unsigned long _lastCalibLedTime = 0;
    bool _calibLedState = false;
    
    // Manual PID state
    float _kp = 1.2f;
    float _ki = 0.02f;
    float _kd = 4.0f;
    float _pVal = 0, _iVal = 0, _dVal = 0;
    float _lastError = 0;
    int _baseSpeed = 150; 
    int _turnSpeed = 120;
    int _sharpTurnSpeed = 120;

    JunctionDetector _detector;
    MissionManager   _mission;
    ControlMode currentControlMode = MODE_MANUAL;
    TurnMode _currentTurnMode = TURN_STRAIGHT;

    unsigned long _lastBtnDebounceTime = 0;
    int _lastBtnState = HIGH;
    int _btnState = HIGH;
    
    int _lastBtnAuto3State = HIGH;
    int _btnAuto3State = HIGH;
    unsigned long _lastBtnAuto3DebounceTime = 0;

    bool _connected = false;

    bool _fwd = false, _bwd = false, _left = false, _right = false;
    bool _isGrabbed = false;
    
    int _lastTurnDirection = 0; 

    // Lost line recovery state
    bool _isLostLine = false;
    bool _isRecovering = false;
    unsigned long _recoveryStartTime = 0;

    // Stuck detection state
    unsigned long _lastStateChangeTime = 0;
    float _lastSignificantError = 0;
    bool _isStuck = false;

    // Finish sequence state
    bool _sawBlackArea = false;
    bool _isBoosting = false;
    unsigned long _boostStartTime = 0;
    bool _finished = false;

    // Start sequence state
    bool _isStarting = false;
    unsigned long _startTimer = 0;
    int _startStep = 0;
    bool _robotRunning = false;
    AutoMission _selectedMission = AutoMission::AUTO_3;
    String _surfaceBits = "000";
    bool _mission2Bit = false;

};

extern RobotLogic robot;
