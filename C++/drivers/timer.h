#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 定时器端口定义
typedef enum {
    TIMER_1 = 0,
    TIMER_2 = 1,
    TIMER_3 = 2,
    TIMER_4 = 3,
    TIMER_5 = 4,
    TIMER_6 = 5,
    TIMER_7 = 6,
    TIMER_8 = 7,
    TIMER_12 = 8,
    TIMER_13 = 9,
    TIMER_14 = 10,
    TIMER_15 = 11,
    TIMER_16 = 12,
    TIMER_17 = 13
} TimerPort;

// 定时器通道定义
typedef enum {
    TIMER_CHANNEL_1 = 0,
    TIMER_CHANNEL_2 = 1,
    TIMER_CHANNEL_3 = 2,
    TIMER_CHANNEL_4 = 3
} TimerChannel;

// 定时器计数模式
typedef enum {
    TIMER_COUNT_UP = 0,
    TIMER_COUNT_DOWN = 1,
    TIMER_COUNT_CENTER_ALIGNED_1 = 2,
    TIMER_COUNT_CENTER_ALIGNED_2 = 3,
    TIMER_COUNT_CENTER_ALIGNED_3 = 4
} TimerCountMode;

// PWM 模式
typedef enum {
    PWM_MODE_FROZEN = 0,
    PWM_MODE_ACTIVE_ON_MATCH = 1,
    PWM_MODE_INACTIVE_ON_MATCH = 2,
    PWM_MODE_TOGGLE = 3,
    PWM_MODE_FORCE_INACTIVE = 4,
    PWM_MODE_FORCE_ACTIVE = 5,
    PWM_MODE_1 = 6,
    PWM_MODE_2 = 7
} PWMMode;

// 定时器配置结构
typedef struct {
    uint32_t prescaler;
    uint32_t period;
    TimerCountMode count_mode;
    bool auto_reload_preload;
    bool one_pulse_mode;
} TimerConfig;

// PWM 配置结构
typedef struct {
    PWMMode mode;
    uint32_t pulse;
    bool output_enable;
    bool output_complementary_enable;
    bool preload_enable;
    bool fast_enable;
} PWMConfig;

// 定时器函数声明
void Timer_Init(TimerPort timer, const TimerConfig* config);
void Timer_DeInit(TimerPort timer);

void Timer_Start(TimerPort timer);
void Timer_Stop(TimerPort timer);

void Timer_Reset(TimerPort timer);

void Timer_SetPrescaler(TimerPort timer, uint32_t prescaler);
void Timer_SetPeriod(TimerPort timer, uint32_t period);

uint32_t Timer_GetCounter(TimerPort timer);
void Timer_SetCounter(TimerPort timer, uint32_t value);

bool Timer_IsUpdateEvent(TimerPort timer);
void Timer_ClearUpdateEvent(TimerPort timer);

// PWM 函数声明
void PWM_Init(TimerPort timer, TimerChannel channel, const PWMConfig* config);
void PWM_DeInit(TimerPort timer, TimerChannel channel);

void PWM_SetPulse(TimerPort timer, TimerChannel channel, uint32_t pulse);
uint32_t PWM_GetPulse(TimerPort timer, TimerChannel channel);

void PWM_EnableOutput(TimerPort timer, TimerChannel channel);
void PWM_DisableOutput(TimerPort timer, TimerChannel channel);

void PWM_EnableComplementaryOutput(TimerPort timer, TimerChannel channel);
void PWM_DisableComplementaryOutput(TimerPort timer, TimerChannel channel);

// 电机控制用的定时器和通道定义
#define MOTOR_LEFT_PWM_TIMER TIMER_2
#define MOTOR_LEFT_PWM_CHANNEL TIMER_CHANNEL_1

#define MOTOR_RIGHT_PWM_TIMER TIMER_2
#define MOTOR_RIGHT_PWM_CHANNEL TIMER_CHANNEL_2

// 系统定时器定义（用于时间戳）
#define SYSTEM_TIMER TIMER_6

// 定时器频率计算辅助宏
#define TIMER_FREQUENCY(Hz) (SYSTEM_CLOCK_FREQ / (Hz) - 1)
#define TIMER_US_TO_TICKS(us) ((us) * (SYSTEM_CLOCK_FREQ / 1000000UL))
#define TIMER_MS_TO_TICKS(ms) ((ms) * (SYSTEM_CLOCK_FREQ / 1000UL))

#ifdef __cplusplus
}
#endif

#endif // TIMER_H
