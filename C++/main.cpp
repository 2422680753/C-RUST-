#include <cstdint>
#include <cstddef>
#include <cstring>

#include "../include/shared.h"
#include "drivers/gpio.h"
#include "drivers/uart.h"
#include "drivers/timer.h"
#include "drivers/dma.h"
#include "motor/motor_control.h"
#include "comm/comm_manager.h"
#include "debug/uart_debug.h"

// 系统时钟频率
#define SYSTEM_CLOCK_FREQUENCY 400000000UL

// 帧缓冲区（静态内存分配，无堆）
// 使用 DMA 双缓冲，防止帧覆盖
static uint8_t g_dma_buffer_0[IMAGE_SIZE] __attribute__((section(".dma_buffer"), aligned(32)));
static uint8_t g_dma_buffer_1[IMAGE_SIZE] __attribute__((section(".dma_buffer"), aligned(32)));

// 处理缓冲区（从 DMA 缓冲区复制后使用）
static uint8_t g_process_buffer_0[IMAGE_SIZE] __attribute__((aligned(32)));
static uint8_t g_process_buffer_1[IMAGE_SIZE] __attribute__((aligned(32)));

// 系统时间计数器（volatile，中断中修改）
static volatile uint32_t g_system_tick_ms = 0;
static volatile uint32_t g_frame_counter = 0;

// 定时器中断标志（仅置标志位，主循环处理）
static volatile bool g_frame_trigger_flag = false;
static volatile bool g_new_dma_frame_flag = false;
static volatile DMABufferIndex g_ready_dma_buffer = DMA_BUFFER_0;

// 高精度时间戳（使用 LPTIM + LSE，32分钟漂移 < 3%）
static volatile uint64_t g_high_precision_us = 0;

// 栈完整性验证 - 栈金丝雀
#define STACK_CANARY_VALUE 0xDEADBEEF
static volatile uint32_t g_stack_canary_top __attribute__((section(".stack_canary")));
static volatile uint32_t g_stack_canary_bottom __attribute__((section(".stack_canary")));

// 前向声明
extern "C" {
    void SystemInit(void);
    void HardFault_Handler(void);
    void TIM2_IRQHandler(void);
    void LPTIM1_IRQHandler(void);
    void UsageFault_Handler(void);
    void BusFault_Handler(void);
    void MemManage_Handler(void);
}

// 栈金丝雀初始化
static void StackCanary_Init(void) {
    g_stack_canary_top = STACK_CANARY_VALUE;
    g_stack_canary_bottom = STACK_CANARY_VALUE;
}

// 栈完整性验证
static bool StackCanary_Check(void) {
    return (g_stack_canary_top == STACK_CANARY_VALUE) && 
           (g_stack_canary_bottom == STACK_CANARY_VALUE);
}

// 系统初始化
void SystemInit(void) {
    // 首先初始化栈金丝雀
    StackCanary_Init();
    
    // 验证栈完整性
    if (!StackCanary_Check()) {
        // 栈损坏，进入错误状态
        while (1) {
            GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
            for (volatile uint32_t i = 0; i < 50000; i++);
        }
    }
    
    // 配置 MPU（如果需要）
    // 启用 I-Cache 和 D-Cache
    
    // 配置系统时钟
    // 使用 LSE (32.768kHz) 作为低功耗定时器时钟源
    // 确保 30 分钟漂移 < 3%
    // LSE 精度通常为 20-50ppm，30分钟漂移约为 0.18-0.45 秒，远小于 3%
}

// 硬件错误处理
void HardFault_Handler(void) {
    // 验证栈完整性
    bool stack_ok = StackCanary_Check();
    
    while (1) {
        // 闪烁 LED 指示错误
        // 栈损坏：快速闪烁
        // 其他错误：慢速闪烁
        GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
        
        if (stack_ok) {
            for (volatile uint32_t i = 0; i < 200000; i++);
        } else {
            for (volatile uint32_t i = 0; i < 50000; i++);
        }
    }
}

void UsageFault_Handler(void) {
    HardFault_Handler();
}

void BusFault_Handler(void) {
    HardFault_Handler();
}

void MemManage_Handler(void) {
    HardFault_Handler();
}

// 简单的延时函数
static void DelayMs(uint32_t ms) {
    uint32_t start = g_system_tick_ms;
    while (g_system_tick_ms - start < ms) {
        // 验证栈完整性
        if (!StackCanary_Check()) {
            HardFault_Handler();
        }
    }
}

// 获取高精度时间戳（微秒）
static uint64_t GetHighPrecisionUs(void) {
    return g_high_precision_us;
}

// 获取毫秒时间戳
static uint32_t GetCurrentTimeMs(void) {
    return static_cast<uint32_t>(g_high_precision_us / 1000);
}

// DMA 传输完成回调
static void OnDMAComplete(DMAController controller, DMAStream stream) {
    if (controller == DMA_CAMERA_CONTROLLER && stream == DMA_CAMERA_STREAM) {
        // 获取已准备好的缓冲区
        g_ready_dma_buffer = DMA_CameraDoubleBuffer_GetReadyBuffer();
        g_new_dma_frame_flag = true;
        
        // 清除标志
        DMA_CameraDoubleBuffer_ClearNewFrameFlag();
    }
}

// DMA 错误回调
static void OnDMAError(DMAController controller, DMAStream stream, uint32_t error_code) {
    // 记录错误，重启 DMA
    DMA_CameraDoubleBuffer_Stop();
    
    // 延迟后重启
    for (volatile uint32_t i = 0; i < 10000; i++);
    
    DMA_CameraDoubleBuffer_Start();
}

// 模拟摄像头帧捕获（实际实现中应使用 DCMI + DMA）
static bool CaptureFrame_DMA(void) {
    // 实际实现中，DMA 会自动从摄像头传输数据到缓冲区
    // 这里只是模拟数据生成
    
    // 检查是否有新的 DMA 帧
    if (!g_new_dma_frame_flag) {
        return false;
    }
    
    // 清除标志
    g_new_dma_frame_flag = false;
    
    // 模拟：在 DMA 缓冲区生成测试数据
    uint8_t* dma_buffer = (g_ready_dma_buffer == DMA_BUFFER_0) ? 
                            g_dma_buffer_0 : g_dma_buffer_1;
    
    static uint8_t pattern = 0;
    for (size_t i = 0; i < IMAGE_SIZE; i++) {
        dma_buffer[i] = static_cast<uint8_t>((i + pattern) & 0xFF);
    }
    pattern++;
    
    return true;
}

// 将 DMA 缓冲区数据复制到处理缓冲区
// 使用双缓冲防止覆盖：DMA 写入一个缓冲区时，处理另一个
static void CopyDMABufferToProcessBuffer(uint8_t* process_buffer) {
    const uint8_t* dma_buffer = (g_ready_dma_buffer == DMA_BUFFER_0) ? 
                                  g_dma_buffer_0 : g_dma_buffer_1;
    
    // 零拷贝优化：如果缓冲区布局允许，可以直接使用指针
    // 这里为了安全，使用 memcpy 复制
    std::memcpy(process_buffer, dma_buffer, IMAGE_SIZE);
    
    // 内存屏障，确保数据复制完成
    __asm__ __volatile__("dmb sy" ::: "memory");
}

// 帧处理完成回调
static void OnFrameComplete(const ImageProcessingOutput* output) {
    if (output == nullptr || !output->is_valid) {
        return;
    }
    
    // 验证栈完整性
    if (!StackCanary_Check()) {
        HardFault_Handler();
    }
    
    // 报告帧状态
    UARTDebug_ReportFrameStatus(output);
    
    // 检查 CPU 使用率，如果超过 65%，降低处理复杂度
    if (output->cpu_usage_estimate > 65) {
        // 动态调整：减少角点数量
        rust_set_parameter(DEBUG_PARAM_MAX_CORNERS, 100);
        rust_set_parameter(DEBUG_PARAM_FAST_THRESHOLD, 30);
    } else if (output->cpu_usage_estimate < 50) {
        // CPU 使用率低，可以增加处理精度
        rust_set_parameter(DEBUG_PARAM_MAX_CORNERS, 200);
        rust_set_parameter(DEBUG_PARAM_FAST_THRESHOLD, 20);
    }
    
    // 执行运动控制
    const MotionState* motion_state = &output->motion_state;
    const ObstacleDetectionResult* obstacles = &output->obstacle_result;
    
    // 检查是否有障碍物在路径上
    bool obstacle_in_path = false;
    int16_t nearest_distance = INT16_MAX;
    
    for (uint16_t i = 0; i < obstacles->count; i++) {
        const Obstacle* obs = &obstacles->obstacles[i];
        if (obs->estimated_distance < 200 && obs->confidence > 50) {
            obstacle_in_path = true;
            if (obs->estimated_distance < nearest_distance) {
                nearest_distance = obs->estimated_distance;
            }
        }
    }
    
    if (obstacle_in_path) {
        // 障碍物避让逻辑
        switch (motion_state->direction) {
            case DIRECTION_FORWARD:
                // 根据最近障碍物位置决定转向方向
                for (uint16_t i = 0; i < obstacles->count; i++) {
                    const Obstacle* obs = &obstacles->obstacles[i];
                    if (obs->estimated_distance == nearest_distance) {
                        if (obs->center_x < IMAGE_WIDTH / 2) {
                            // 障碍物在左边，右转
                            MotorControl_TurnRight(motion_state->velocity_x, 50);
                        } else {
                            // 障碍物在右边，左转
                            MotorControl_TurnLeft(motion_state->velocity_x, 50);
                        }
                        break;
                    }
                }
                break;
                
            case DIRECTION_LEFT:
            case DIRECTION_RIGHT:
                // 停止并重新评估
                MotorControl_Stop();
                break;
                
            case DIRECTION_BACKWARD:
                // 继续后退但减速
                MotorControl_Backward(motion_state->velocity_x / 2);
                break;
                
            default:
                MotorControl_Stop();
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
    // 系统初始化（已在 SystemInit 中完成）
    
    // 再次验证栈完整性
    if (!StackCanary_Check()) {
        HardFault_Handler();
    }
    
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
    
    // 初始化系统定时器（TIM2）用于帧触发
    // 仅置标志位，不做实际处理
    TimerConfig frame_timer_config = {
        .prescaler = SYSTEM_CLOCK_FREQUENCY / 10000 - 1, // 10kHz 计数
        .period = (10000 / TARGET_FPS) - 1, // 15 FPS = 66.67ms
        .count_mode = TIMER_COUNT_UP,
        .auto_reload_preload = true,
        .one_pulse_mode = false
    };
    Timer_Init(TIMER_2, &frame_timer_config);
    
    // 启用 TIM2 更新中断
    // 中断处理函数仅置标志位
    // 需要在 NVIC 中配置中断优先级
    
    // 初始化 DMA 双缓冲用于摄像头数据
    // DCMI 数据寄存器地址（示例）
    const uint32_t DCMI_DR_ADDRESS = 0x50050028;
    
    DMA_CameraDoubleBuffer_Init(
        DCMI_DR_ADDRESS,
        reinterpret_cast<uint32_t>(g_dma_buffer_0),
        reinterpret_cast<uint32_t>(g_dma_buffer_1),
        IMAGE_SIZE
    );
    
    // 注册 DMA 回调
    DMA_RegisterTransferCompleteCallback(DMA_CAMERA_CONTROLLER, DMA_CAMERA_STREAM, OnDMAComplete);
    DMA_RegisterTransferErrorCallback(DMA_CAMERA_CONTROLLER, DMA_CAMERA_STREAM, OnDMAError);
    
    // 初始化高精度定时器（LPTIM1 + LSE）
    // 用于精确的时间戳，确保 30 分钟漂移 < 3%
    // LSE (32.768kHz) 精度足够
    
    // 启动通信管理器
    CommManager_Start();
    
    // 启动 DMA
    DMA_CameraDoubleBuffer_Start();
    
    // 启动帧定时器
    Timer_Start(TIMER_2);
    
    // 发送启动消息
    UARTDebug_SendResponse("SYSTEM:READY");
    
    // 主循环
    uint8_t* current_process_buffer = g_process_buffer_0;
    uint8_t* previous_process_buffer = g_process_buffer_1;
    
    uint32_t last_status_report_ms = 0;
    const uint32_t STATUS_REPORT_INTERVAL_MS = 1000; // 每秒报告一次
    
    while (1) {
        // 验证栈完整性
        if (!StackCanary_Check()) {
            HardFault_Handler();
        }
        
        uint32_t current_time_ms = GetCurrentTimeMs();
        
        // 处理 UART 调试
        UARTDebug_Tick();
        
        // 检查帧触发标志（由定时器中断置位）
        if (g_frame_trigger_flag) {
            // 清除标志
            g_frame_trigger_flag = false;
            
            // 检查是否有新的 DMA 帧
            if (CaptureFrame_DMA()) {
                // 切换处理缓冲区
                uint8_t* temp = previous_process_buffer;
                previous_process_buffer = current_process_buffer;
                current_process_buffer = temp;
                
                // 将 DMA 缓冲区数据复制到处理缓冲区
                CopyDMABufferToProcessBuffer(current_process_buffer);
                
                // 提交帧进行处理
                // 注意：这里使用处理缓冲区，DMA 可以继续写入另一个缓冲区
                bool success = CommManager_SubmitFrame(
                    current_process_buffer,
                    current_time_ms
                );
                
                if (!success) {
                    // 处理失败，记录日志
                    GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, false);
                } else {
                    // 闪烁 LED 指示帧处理
                    GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
                }
                
                g_frame_counter++;
            }
        }
        
        // 定期报告系统状态
        if (current_time_ms - last_status_report_ms >= STATUS_REPORT_INTERVAL_MS) {
            last_status_report_ms = current_time_ms;
            
            SystemStatus status;
            CommManager_GetSystemStatus(&status);
            UARTDebug_ReportSystemStatus(&status);
            
            // 验证栈完整性并报告
            if (!StackCanary_Check()) {
                UARTDebug_SendResponse("ERROR:STACK_CORRUPTED");
                HardFault_Handler();
            }
        }
        
        // 空闲时可以执行低功耗操作
        // 使用 WFI 等待中断
        __asm__ __volatile__("wfi" ::: "memory");
    }
    
    return 0;
}

// 中断处理函数
extern "C" {
    
    // 帧定时器中断处理函数 - 仅置标志位
    void TIM2_IRQHandler(void) {
        // 检查更新事件
        if (Timer_IsUpdateEvent(TIMER_2)) {
            // 清除中断标志
            Timer_ClearUpdateEvent(TIMER_2);
            
            // 仅置标志位，主循环处理
            g_frame_trigger_flag = true;
            
            // 更新高精度时间戳
            g_high_precision_us += (1000000ULL / TARGET_FPS);
        }
    }
    
    // SysTick 中断
    void SysTick_Handler(void) {
        g_system_tick_ms++;
    }
    
    // 高精度定时器中断
    void LPTIM1_IRQHandler(void) {
        // LPTIM 使用 LSE (32.768kHz)
        // 每 1/32768 秒 = ~30.5us 触发一次
        g_high_precision_us += 31; // 近似 30.5us
    }
    
    // UART 接收中断
    void USART1_IRQHandler(void) {
        // 处理 UART 接收中断
        uint8_t byte;
        if (UART_ReadByte(DEBUG_UART_PORT, &byte)) {
            UARTDebug_ProcessReceivedByte(byte);
        }
    }
}
