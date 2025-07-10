#include "arm.h"

extern TIM_HandleTypeDef htim3; // ?? CubeMX ??????

#define SERVO_TIM      htim3
#define SERVO_CHANNEL  TIM_CHANNEL_1

#define SERVO_MIN_PULSE 500    // 0??????,??us
#define SERVO_MAX_PULSE 2500   // 180??????,??us

void Servo_Init(void)
{
    HAL_TIM_PWM_Start(&SERVO_TIM, SERVO_CHANNEL);
}

// ????(0~180)
void Servo_SetAngle(uint8_t angle)
{
    if (angle > 180) angle = 180;

    uint16_t pulse = SERVO_MIN_PULSE + ((SERVO_MAX_PULSE - SERVO_MIN_PULSE) * angle) / 180;

    __HAL_TIM_SET_COMPARE(&SERVO_TIM, SERVO_CHANNEL, pulse);
}