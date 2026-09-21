#pragma once

class HcSr04 {
public:
    void  begin();
    float getDistance(); // cm

private:
    unsigned long _lastPing = 0;
    float         _lastDist = 999.0f;
};

extern HcSr04 ultrasonic;
