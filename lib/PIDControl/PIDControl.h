#pragma once

class PIDControl {
private:
    float Kp;
    float Ki;
    float integral;
    float out_min;
    float out_max;

public:
    PIDControl(float p, float i, float min_val, float max_val);
    
    void setParams(float p, float i);
    void reset();
    
    float compute(float setpoint, float measured, float dt, bool& is_saturated);
};