#include "filter.h"

LowPassFilter::LowPassFilter(float alpha) {
    _alpha  = alpha;
    _output = 0.0f;
}

float LowPassFilter::update(float input) {
    _output = (_alpha * input) + ((1.0f - _alpha) * _output);
    return _output;
}

void LowPassFilter::reset(float value) {
    _output = value;
}
