#include "PIDControl.h"

PIDControl::PIDControl(float p, float i, float min_val, float max_val) 
    : Kp(p), Ki(i), integral(0.0f), out_min(min_val), out_max(max_val) {}

void PIDControl::setParams(float p, float i) {
    Kp = p;
    Ki = i;
}

void PIDControl::reset() {
    integral = 0.0f;
}

float PIDControl::compute(float setpoint, float measured, float dt, bool& is_saturated) {
    float error = setpoint - measured;
    float p_term = Kp * error;
    
    float temp_out = p_term + (Ki * integral);
    
    is_saturated = false;

    if ((temp_out < out_max && temp_out > out_min) || 
        (temp_out >= out_max && error < 0.0f) || 
        (temp_out <= out_min && error > 0.0f)) {
        integral += error * dt;
    }
    
    float output = p_term + (Ki * integral);
    
    if (output >= out_max) {
        output = out_max;
        if (error > 20.0f) {
            is_saturated = true;
        }
    } else if (output <= out_min) {
        output = out_min;
    }
    
    return output;
}