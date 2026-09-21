#pragma once

class Motor {
public:
    void begin();
    void setSpeed(int left, int right);
    void stop();
};

extern Motor motor;
