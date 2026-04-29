#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "../../include/shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// 电机速度范围定义
#define MOTOR_MIN_SPEED 0
#define MOTOR_MAX_SPEED 1000
#define MOTOR_MAX_PWM 999

// 电机方向定义
typedef enum {
    MOTOR_DIRECTION_FORWARD = 0,
    MOTOR_DIRECTION_BACKWARD = 1,
    MOTOR_DIRECTION_STOP = 2
} MotorDirection;

// 电机状态结构
typedef struct {
    int16_t left_speed;
    int16_t right_speed;
    MotorDirection left_direction;
    MotorDirection right_direction;
    bool enabled;
    uint32_t last_update_ms;
} MotorState;

// 电机控制函数声明
void MotorControl_Init(void);
void MotorControl_DeInit(void);

void MotorControl_Enable(void);
void MotorControl_Disable(void);

void MotorControl_SetSpeed(int16_t left_speed, int16_t right_speed);
void MotorControl_SetLeftSpeed(int16_t speed);
void MotorControl_SetRightSpeed(int16_t speed);

void MotorControl_SetDirection(MotorDirection left_dir, MotorDirection right_dir);
void MotorControl_SetLeftDirection(MotorDirection dir);
void MotorControl_SetRightDirection(MotorDirection dir);

void MotorControl_Stop(void);
void MotorControl_StopSoft(void);

void MotorControl_Forward(int16_t speed);
void MotorControl_Backward(int16_t speed);
void MotorControl_TurnLeft(int16_t speed, int16_t turn_rate);
void MotorControl_TurnRight(int16_t speed, int16_t turn_rate);
void MotorControl_RotateLeft(int16_t speed);
void MotorControl_RotateRight(int16_t speed);

void MotorControl_ExecuteMotionState(const MotionState* state);

void MotorControl_GetState(MotorState* state);
void MotorControl_GetMotorControlState(MotorControlState* mcs);

// 辅助函数
int16_t MotorControl_ClampSpeed(int16_t speed);
uint32_t MotorControl_SpeedToPWM(int16_t speed);

#ifdef __cplusplus
}
#endif

#endif // MOTOR_CONTROL_H
