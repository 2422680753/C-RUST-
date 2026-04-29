#include "motor_control.h"
#include "../drivers/gpio.h"
#include "../drivers/timer.h"

// 电机状态的静态存储（无堆内存）
static MotorState g_motor_state = {
    .left_speed = 0,
    .right_speed = 0,
    .left_direction = MOTOR_DIRECTION_STOP,
    .right_direction = MOTOR_DIRECTION_STOP,
    .enabled = false,
    .last_update_ms = 0
};

// 获取当前时间戳（毫秒）
static uint32_t GetCurrentTimeMs(void) {
    // 使用系统定时器或其他方式获取时间
    // 这里假设 TIMER_6 被配置为微秒计数器
    return Timer_GetCounter(SYSTEM_TIMER) / 1000;
}

// 初始化电机控制
void MotorControl_Init(void) {
    // 配置电机 PWM 引脚的 GPIO
    GPIOConfig pwm_config = {
        .mode = GPIO_MODE_AF,
        .output_type = GPIO_OUTPUT_TYPE_PUSH_PULL,
        .speed = GPIO_SPEED_HIGH,
        .pull_up_down = GPIO_PUPD_NO_PULL,
        .alternate_function = 1 // TIM2 的 AF1
    };
    
    GPIO_Init(MOTOR_LEFT_PWM_PORT, MOTOR_LEFT_PWM_PIN, &pwm_config);
    GPIO_Init(MOTOR_RIGHT_PWM_PORT, MOTOR_RIGHT_PWM_PIN, &pwm_config);
    
    // 配置电机方向引脚的 GPIO
    GPIOConfig dir_config = {
        .mode = GPIO_MODE_OUTPUT,
        .output_type = GPIO_OUTPUT_TYPE_PUSH_PULL,
        .speed = GPIO_SPEED_LOW,
        .pull_up_down = GPIO_PUPD_NO_PULL,
        .alternate_function = 0
    };
    
    GPIO_Init(MOTOR_LEFT_DIR_PORT, MOTOR_LEFT_DIR_PIN, &dir_config);
    GPIO_Init(MOTOR_RIGHT_DIR_PORT, MOTOR_RIGHT_DIR_PIN, &dir_config);
    
    // 初始化电机方向引脚为低电平（停止状态）
    GPIO_WritePin(MOTOR_LEFT_DIR_PORT, MOTOR_LEFT_DIR_PIN, false);
    GPIO_WritePin(MOTOR_RIGHT_DIR_PORT, MOTOR_RIGHT_DIR_PIN, false);
    
    // 配置定时器用于 PWM
    TimerConfig timer_config = {
        .prescaler = 0, // 不分频
        .period = MOTOR_MAX_PWM, // 1kHz PWM 频率（假设 1MHz 定时器时钟）
        .count_mode = TIMER_COUNT_UP,
        .auto_reload_preload = true,
        .one_pulse_mode = false
    };
    
    Timer_Init(MOTOR_LEFT_PWM_TIMER, &timer_config);
    
    // 配置 PWM 通道
    PWMConfig pwm_channel_config = {
        .mode = PWM_MODE_1,
        .pulse = 0,
        .output_enable = true,
        .output_complementary_enable = false,
        .preload_enable = true,
        .fast_enable = false
    };
    
    PWM_Init(MOTOR_LEFT_PWM_TIMER, MOTOR_LEFT_PWM_CHANNEL, &pwm_channel_config);
    PWM_Init(MOTOR_RIGHT_PWM_TIMER, MOTOR_RIGHT_PWM_CHANNEL, &pwm_channel_config);
    
    // 启动定时器
    Timer_Start(MOTOR_LEFT_PWM_TIMER);
    
    // 初始化电机状态
    g_motor_state.enabled = false;
    g_motor_state.left_speed = 0;
    g_motor_state.right_speed = 0;
    g_motor_state.left_direction = MOTOR_DIRECTION_STOP;
    g_motor_state.right_direction = MOTOR_DIRECTION_STOP;
    g_motor_state.last_update_ms = GetCurrentTimeMs();
}

void MotorControl_DeInit(void) {
    // 停止电机
    MotorControl_Stop();
    
    // 禁用 PWM 输出
    PWM_DisableOutput(MOTOR_LEFT_PWM_TIMER, MOTOR_LEFT_PWM_CHANNEL);
    PWM_DisableOutput(MOTOR_RIGHT_PWM_TIMER, MOTOR_RIGHT_PWM_CHANNEL);
    
    // 停止定时器
    Timer_Stop(MOTOR_LEFT_PWM_TIMER);
    
    // 反初始化 GPIO
    GPIO_DeInit(MOTOR_LEFT_PWM_PORT, MOTOR_LEFT_PWM_PIN);
    GPIO_DeInit(MOTOR_RIGHT_PWM_PORT, MOTOR_RIGHT_PWM_PIN);
    GPIO_DeInit(MOTOR_LEFT_DIR_PORT, MOTOR_LEFT_DIR_PIN);
    GPIO_DeInit(MOTOR_RIGHT_DIR_PORT, MOTOR_RIGHT_DIR_PIN);
    
    // 更新状态
    g_motor_state.enabled = false;
}

void MotorControl_Enable(void) {
    g_motor_state.enabled = true;
    
    // 启用 PWM 输出
    PWM_EnableOutput(MOTOR_LEFT_PWM_TIMER, MOTOR_LEFT_PWM_CHANNEL);
    PWM_EnableOutput(MOTOR_RIGHT_PWM_TIMER, MOTOR_RIGHT_PWM_CHANNEL);
}

void MotorControl_Disable(void) {
    g_motor_state.enabled = false;
    
    // 先停止电机
    MotorControl_Stop();
    
    // 禁用 PWM 输出
    PWM_DisableOutput(MOTOR_LEFT_PWM_TIMER, MOTOR_LEFT_PWM_CHANNEL);
    PWM_DisableOutput(MOTOR_RIGHT_PWM_TIMER, MOTOR_RIGHT_PWM_CHANNEL);
}

void MotorControl_SetSpeed(int16_t left_speed, int16_t right_speed) {
    MotorControl_SetLeftSpeed(left_speed);
    MotorControl_SetRightSpeed(right_speed);
}

void MotorControl_SetLeftSpeed(int16_t speed) {
    // 限制速度范围
    speed = MotorControl_ClampSpeed(speed);
    
    // 根据速度符号设置方向
    if (speed > 0) {
        MotorControl_SetLeftDirection(MOTOR_DIRECTION_FORWARD);
    } else if (speed < 0) {
        MotorControl_SetLeftDirection(MOTOR_DIRECTION_BACKWARD);
        speed = -speed; // 取绝对值用于 PWM
    } else {
        MotorControl_SetLeftDirection(MOTOR_DIRECTION_STOP);
    }
    
    g_motor_state.left_speed = speed;
    
    // 设置 PWM
    uint32_t pwm = MotorControl_SpeedToPWM(speed);
    PWM_SetPulse(MOTOR_LEFT_PWM_TIMER, MOTOR_LEFT_PWM_CHANNEL, pwm);
    
    g_motor_state.last_update_ms = GetCurrentTimeMs();
}

void MotorControl_SetRightSpeed(int16_t speed) {
    // 限制速度范围
    speed = MotorControl_ClampSpeed(speed);
    
    // 根据速度符号设置方向
    if (speed > 0) {
        MotorControl_SetRightDirection(MOTOR_DIRECTION_FORWARD);
    } else if (speed < 0) {
        MotorControl_SetRightDirection(MOTOR_DIRECTION_BACKWARD);
        speed = -speed; // 取绝对值用于 PWM
    } else {
        MotorControl_SetRightDirection(MOTOR_DIRECTION_STOP);
    }
    
    g_motor_state.right_speed = speed;
    
    // 设置 PWM
    uint32_t pwm = MotorControl_SpeedToPWM(speed);
    PWM_SetPulse(MOTOR_RIGHT_PWM_TIMER, MOTOR_RIGHT_PWM_CHANNEL, pwm);
    
    g_motor_state.last_update_ms = GetCurrentTimeMs();
}

void MotorControl_SetDirection(MotorDirection left_dir, MotorDirection right_dir) {
    MotorControl_SetLeftDirection(left_dir);
    MotorControl_SetRightDirection(right_dir);
}

void MotorControl_SetLeftDirection(MotorDirection dir) {
    g_motor_state.left_direction = dir;
    
    switch (dir) {
        case MOTOR_DIRECTION_FORWARD:
            GPIO_WritePin(MOTOR_LEFT_DIR_PORT, MOTOR_LEFT_DIR_PIN, false);
            break;
        case MOTOR_DIRECTION_BACKWARD:
            GPIO_WritePin(MOTOR_LEFT_DIR_PORT, MOTOR_LEFT_DIR_PIN, true);
            break;
        case MOTOR_DIRECTION_STOP:
        default:
            // 停止时可以保持当前状态或设置为高阻
            break;
    }
}

void MotorControl_SetRightDirection(MotorDirection dir) {
    g_motor_state.right_direction = dir;
    
    switch (dir) {
        case MOTOR_DIRECTION_FORWARD:
            GPIO_WritePin(MOTOR_RIGHT_DIR_PORT, MOTOR_RIGHT_DIR_PIN, false);
            break;
        case MOTOR_DIRECTION_BACKWARD:
            GPIO_WritePin(MOTOR_RIGHT_DIR_PORT, MOTOR_RIGHT_DIR_PIN, true);
            break;
        case MOTOR_DIRECTION_STOP:
        default:
            // 停止时可以保持当前状态或设置为高阻
            break;
    }
}

void MotorControl_Stop(void) {
    // 立即停止
    PWM_SetPulse(MOTOR_LEFT_PWM_TIMER, MOTOR_LEFT_PWM_CHANNEL, 0);
    PWM_SetPulse(MOTOR_RIGHT_PWM_TIMER, MOTOR_RIGHT_PWM_CHANNEL, 0);
    
    g_motor_state.left_speed = 0;
    g_motor_state.right_speed = 0;
    g_motor_state.left_direction = MOTOR_DIRECTION_STOP;
    g_motor_state.right_direction = MOTOR_DIRECTION_STOP;
    g_motor_state.last_update_ms = GetCurrentTimeMs();
}

void MotorControl_StopSoft(void) {
    // 软停止 - 逐渐降低速度
    int16_t left_speed = g_motor_state.left_speed;
    int16_t right_speed = g_motor_state.right_speed;
    
    while (left_speed > 0 || right_speed > 0) {
        if (left_speed > 0) {
            left_speed--;
        }
        if (right_speed > 0) {
            right_speed--;
        }
        
        MotorControl_SetLeftSpeed(left_speed);
        MotorControl_SetRightSpeed(right_speed);
        
        // 简单的延时
        for (volatile uint32_t i = 0; i < 100; i++);
    }
    
    MotorControl_Stop();
}

void MotorControl_Forward(int16_t speed) {
    speed = MotorControl_ClampSpeed(speed);
    MotorControl_SetDirection(MOTOR_DIRECTION_FORWARD, MOTOR_DIRECTION_FORWARD);
    MotorControl_SetSpeed(speed, speed);
}

void MotorControl_Backward(int16_t speed) {
    speed = MotorControl_ClampSpeed(speed);
    MotorControl_SetDirection(MOTOR_DIRECTION_BACKWARD, MOTOR_DIRECTION_BACKWARD);
    MotorControl_SetSpeed(-speed, -speed);
}

void MotorControl_TurnLeft(int16_t speed, int16_t turn_rate) {
    speed = MotorControl_ClampSpeed(speed);
    turn_rate = MotorControl_ClampSpeed(turn_rate);
    
    int16_t left_speed = speed - turn_rate;
    int16_t right_speed = speed + turn_rate;
    
    // 限制速度范围
    left_speed = MotorControl_ClampSpeed(left_speed);
    right_speed = MotorControl_ClampSpeed(right_speed);
    
    MotorControl_SetDirection(MOTOR_DIRECTION_FORWARD, MOTOR_DIRECTION_FORWARD);
    MotorControl_SetSpeed(left_speed, right_speed);
}

void MotorControl_TurnRight(int16_t speed, int16_t turn_rate) {
    speed = MotorControl_ClampSpeed(speed);
    turn_rate = MotorControl_ClampSpeed(turn_rate);
    
    int16_t left_speed = speed + turn_rate;
    int16_t right_speed = speed - turn_rate;
    
    // 限制速度范围
    left_speed = MotorControl_ClampSpeed(left_speed);
    right_speed = MotorControl_ClampSpeed(right_speed);
    
    MotorControl_SetDirection(MOTOR_DIRECTION_FORWARD, MOTOR_DIRECTION_FORWARD);
    MotorControl_SetSpeed(left_speed, right_speed);
}

void MotorControl_RotateLeft(int16_t speed) {
    speed = MotorControl_ClampSpeed(speed);
    MotorControl_SetDirection(MOTOR_DIRECTION_BACKWARD, MOTOR_DIRECTION_FORWARD);
    MotorControl_SetSpeed(-speed, speed);
}

void MotorControl_RotateRight(int16_t speed) {
    speed = MotorControl_ClampSpeed(speed);
    MotorControl_SetDirection(MOTOR_DIRECTION_FORWARD, MOTOR_DIRECTION_BACKWARD);
    MotorControl_SetSpeed(speed, -speed);
}

void MotorControl_ExecuteMotionState(const MotionState* state) {
    if (state == nullptr) {
        return;
    }
    
    switch (state->direction) {
        case DIRECTION_FORWARD:
            MotorControl_Forward(state->velocity_x);
            break;
        case DIRECTION_BACKWARD:
            MotorControl_Backward(state->velocity_x);
            break;
        case DIRECTION_LEFT:
            if (state->velocity_x > 0) {
                MotorControl_TurnLeft(state->velocity_x, state->rotation);
            } else {
                MotorControl_RotateLeft(state->rotation);
            }
            break;
        case DIRECTION_RIGHT:
            if (state->velocity_x > 0) {
                MotorControl_TurnRight(state->velocity_x, state->rotation);
            } else {
                MotorControl_RotateRight(state->rotation);
            }
            break;
        case DIRECTION_STOP:
        default:
            MotorControl_Stop();
            break;
    }
}

void MotorControl_GetState(MotorState* state) {
    if (state == nullptr) {
        return;
    }
    
    *state = g_motor_state;
}

void MotorControl_GetMotorControlState(MotorControlState* mcs) {
    if (mcs == nullptr) {
        return;
    }
    
    mcs->left_speed = g_motor_state.left_speed;
    mcs->right_speed = g_motor_state.right_speed;
    mcs->left_forward = (g_motor_state.left_direction == MOTOR_DIRECTION_FORWARD);
    mcs->right_forward = (g_motor_state.right_direction == MOTOR_DIRECTION_FORWARD);
    mcs->timestamp_ms = GetCurrentTimeMs();
}

int16_t MotorControl_ClampSpeed(int16_t speed) {
    if (speed > MOTOR_MAX_SPEED) {
        return MOTOR_MAX_SPEED;
    } else if (speed < -MOTOR_MAX_SPEED) {
        return -MOTOR_MAX_SPEED;
    }
    return speed;
}

uint32_t MotorControl_SpeedToPWM(int16_t speed) {
    // 确保速度为正
    if (speed < 0) {
        speed = -speed;
    }
    
    // 限制速度范围
    if (speed > MOTOR_MAX_SPEED) {
        speed = MOTOR_MAX_SPEED;
    }
    
    // 线性映射速度到 PWM
    // speed: 0-1000 -> PWM: 0-999
    return static_cast<uint32_t>(speed);
}
