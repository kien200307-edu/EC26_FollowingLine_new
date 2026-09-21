#pragma once

class PID {
public:
    PID(float kp, float ki, float kd);

    float compute(float error);

    void setTunings(float kp, float ki, float kd);
    void setKp(float value);
    void setKi(float value);
    void setKd(float value);

    float getKp() const { return _kp; }
    float getKi() const { return _ki; }
    float getKd() const { return _kd; }

    void reset();
    void leakIntegral(float factor);

private:
    float _kp, _ki, _kd;
    float integral;
    float previousError;
};
