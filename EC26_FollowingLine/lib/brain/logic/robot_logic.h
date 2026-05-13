#pragma once
#include <Arduino.h>
#include "utils/pid.h"
#include "junction_detector.h"
#include "mission_manager.h"

enum ControlMode {
  MODE_MANUAL,
  MODE_AUTOMATION
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
    void setBaseSpeed(int speed);
    void setPidKp(float v);
    void setPidKi(float v);
    void setPidKd(float v);
    void applyParams(float p, float i, float d, int speed);

    void setPid45Kp(float v);
    void setPid45Ki(float v);
    void setPid45Kd(float v);

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
    void followLinePID();
    void normalPID();
    void turnLeft90();
    void turnRight90();
    void manualDrive();
    void autoGrabLogic();
    void rescueLogic();
    void updateLED();
    void updateControlModeButton();

    PID* _pid = nullptr;
    PID* _pid45 = nullptr; 
    JunctionDetector _detector;
    MissionManager   _mission;
    ControlMode currentControlMode = MODE_MANUAL;
    unsigned long _lastBtnDebounceTime = 0;
    int _lastBtnState = HIGH;
    int _btnState = HIGH;
    
    int _lastBtnAuto3State = HIGH;
    int _btnAuto3State = HIGH;
    unsigned long _lastBtnAuto3DebounceTime = 0;


    bool _connected = false;
    int  _baseSpeed = PID_BASE_SPEED_DEFAULT; 

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
