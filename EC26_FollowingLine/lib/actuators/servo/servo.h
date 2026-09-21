#pragma once

class ServoControl {
public:
    void begin();
    void update();
    void setLeft(int angle);
    void setRight(int angle);
    void open();   // Thả đồ
    void close();  // Gắp đồ

private:
    int leftAngle  = 90;
    int rightAngle = 0;
    
    int targetLeftAngle = 90;
    int targetRightAngle = 0;
    unsigned long lastServoUpdate = 0;
};

extern ServoControl servoControl;
