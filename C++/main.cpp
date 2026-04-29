#include <cstdint>
#include <cstddef>
#include <cstring>

#include "../include/shared.h"
#include "drivers/gpio.h"
#include "drivers/uart.h"
#include "drivers/timer.h"
#include "motor/motor_control.h"
#include "comm/comm_manager.h"
#include "debug/uart_debug.h"

// 系统时钟频率
#define SYSTEM_CLOCK_FREQUENCY 400000000UL

// 帧缓冲区（静态内存分配，无堆）
static uint8_t g_frame_buffer_0[IMAGE_SIZE];
static uint8_t g_frame_buffer_1[IMAGE_SIZE];
static uint8_t* g_current_frame_buffer = g_frame_buffer_0;
static uint8_t* g_previous_frame_buffer = g_frame_buffer_1;

// 系统时间计数器
static volatile uint32_t g_system_tick_ms = 0;
static volatile uint32_t g_frame_counter = 0;

// 前向声明
extern "C" {
    void SystemInit(void);
    void HardFault_Handler(void);
}

// 系统初始化
void SystemInit(void) {
    // 这里应该配置系统时钟、MPU、缓存等
    // 简化实现
}

// 硬件错误处理
void HardFault_Handler(void) {
    while (1) {
        // 闪烁 LED 指示错误
        GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
        for (volatile uint32_t i = 0; i < 100000; i++);
    }
}

// 简单的延时函数
static void DelayMs(uint32_t ms) {
    uint32_t start = g_system_tick_ms;
    while (g_system_tick_ms - start < ms) {
        // 等待
    }
}

// 模拟摄像头帧捕获（实际实现中应使用 DCMI 或摄像头接口）
static bool CaptureFrame(uint8_t* buffer) {
    if (buffer == nullptr) {
        return false;
    }
    
    // 生成模拟测试数据
    // 实际实现中应从摄像头获取数据
    static uint8_t pattern = 0;
    for (size_t i = 0; i < IMAGE_SIZE; i++) {
        buffer[i] = static_cast<uint8_t>((i + pattern) & 0xFF);
    }
    pattern++;
    
    return true;
}

// 帧处理完成回调
static void OnFrameComplete(const ImageProcessingOutput* output) {
    if (output == nullptr || !output->is_valid) {
        return;
    }
    
    // 报告帧状态
    UARTDebug_ReportFrameStatus(output);
    
    // 检查 CPU 使用率
    if (output->cpu_usage_estimate > 65) {
        // CPU 使用率过高，可以考虑降低处理复杂度
        // 例如：减少角点数量、降低检测频率等
    }
    
    // 执行运动控制
    const MotionState* motion_state = &output->motion_state;
    const ObstacleDetectionResult* obstacles = &output->obstacle_result;
    
    // 检查是否有障碍物在路径上
    bool obstacle_in_path = false;
    for (uint16_t i = 0; i < obstacles->count; i++) {
        const Obstacle* obs = &obstacles->obstacles[i];
        if (obs->estimated_distance < 200 && obs->confidence > 50) {
            obstacle_in_path = true;
            break;
        }
    }
    
    if (obstacle_in_path) {
        // 障碍物避让逻辑
        switch (motion_state->direction) {
            case DIRECTION_FORWARD:
                // 尝试左转或右转
                MotorControl_TurnLeft(motion_state->velocity_x, 50);
                break;
            case DIRECTION_LEFT:
            case DIRECTION_RIGHT:
                // 停止
                MotorControl_Stop();
                break;
            default:
                break;
        }
    } else {
        // 正常执行运动
        MotorControl_ExecuteMotionState(motion_state);
    }
    
    // 报告检测到的障碍物
    if (obstacles->count > 0) {
        UARTDebug_ReportObstacles(obstacles);
    }
    
    // 报告运动状态
    UARTDebug_ReportMotion(motion_state);
}

// 主函数
int main(void) {
    // 系统初始化
    SystemInit();
    
    // 初始化 GPIO 配置
    GPIOConfig led_config = {
        .mode = GPIO_MODE_OUTPUT,
        .output_type = GPIO_OUTPUT_TYPE_PUSH_PULL,
        .speed = GPIO_SPEED_LOW,
        .pull_up_down = GPIO_PUPD_NO_PULL,
        .alternate_function = 0
    };
    GPIO_Init(LED_STATUS_PORT, LED_STATUS_PIN, &led_config);
    GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, false);
    
    // 初始化通信管理器
    CommManager_Init();
    CommManager_RegisterFrameCompleteCallback(OnFrameComplete);
    
    // 初始化电机控制
    MotorControl_Init();
    MotorControl_Enable();
    
    // 初始化 UART 调试
    UARTDebug_Init();
    UARTDebug_Start();
    
    // 初始化系统定时器
    TimerConfig timer_config = {
        .prescaler = SYSTEM_CLOCK_FREQUENCY / 1000 - 1, // 1ms  tick
        .period = 0xFFFFFFFF,
        .count_mode = TIMER_COUNT_UP,
        .auto_reload_preload = false,
        .one_pulse_mode = false
    };
    Timer_Init(SYSTEM_TIMER, &timer_config);
    Timer_Start(SYSTEM_TIMER);
    
    // 启动通信管理器
    CommManager_Start();
    
    // 发送启动消息
    UARTDebug_SendResponse("SYSTEM:READY");
    
    // 主循环
    uint32_t last_frame_time = 0;
    uint32_t frame_interval = FRAME_INTERVAL_MS;
    
    while (1) {
        uint32_t current_time = Timer_GetCounter(SYSTEM_TIMER) / 1000;
        
        // 处理 UART 调试
        UARTDebug_Tick();
        
        // 检查是否到达帧时间
        if (current_time - last_frame_time >= frame_interval) {
            last_frame_time = current_time;
            
            // 切换帧缓冲区
            uint8_t* temp = g_previous_frame_buffer;
            g_previous_frame_buffer = g_current_frame_buffer;
            g_current_frame_buffer = temp;
            
            // 捕获新帧
            if (CaptureFrame(g_current_frame_buffer)) {
                // 提交帧进行处理
                bool success = CommManager_SubmitFrame(
                    g_current_frame_buffer,
                    current_time
                );
                
                if (!success) {
                    // 处理失败，可以记录日志
                    GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, false);
                } else {
                    // 闪烁 LED 指示帧处理
                    GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
                }
                
                g_frame_counter++;
            }
        }
        
        // 空闲时可以执行低功耗操作
        __asm__ __volatile__("wfi" ::: "memory");
    }
    
    return 0;
}

// 中断处理函数（如果需要）
extern "C" {
    void SysTick_Handler(void) {
        g_system_tick_ms++;
    }
    
    void USART1_IRQHandler(void) {
        // 处理 UART 接收中断
        uint8_t byte;
        if (UART_ReadByte(DEBUG_UART_PORT, &byte)) {
            UARTDebug_ProcessReceivedByte(byte);
        }
    }
}
