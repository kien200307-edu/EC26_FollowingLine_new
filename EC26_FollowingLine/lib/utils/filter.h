#pragma once

class LowPassFilter {
public:
    LowPassFilter(float alpha = 0.5f);
    float update(float input);
    void  reset(float value = 0.0f);

private:
    float _alpha;
    float _output;
};
