#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#include <stdint.h>
#include <stdbool.h>
#include "../../include/shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// 调试命令缓冲区大小
#define DEBUG_COMMAND_BUFFER_SIZE 64
#define DEBUG_RESPONSE_BUFFER_SIZE 128

// 调试命令前缀
#define DEBUG_COMMAND_PREFIX '#'
#define DEBUG_RESPONSE_PREFIX '*'

// 调试状态枚举
typedef enum {
    DEBUG_STATE_IDLE = 0,
    DEBUG_STATE_RECEIVING = 1,
    DEBUG_STATE_PROCESSING = 2,
    DEBUG_STATE_RESPONDING = 3,
    DEBUG_STATE_ERROR = 4
} DebugState;

// 调试命令解析结果
typedef struct {
    DebugCommand command;
    bool is_valid;
    uint8_t error_code;
} DebugCommandParseResult;

// 调试管理器状态
typedef struct {
    DebugState current_state;
    uint8_t rx_buffer[DEBUG_COMMAND_BUFFER_SIZE];
    uint8_t tx_buffer[DEBUG_RESPONSE_BUFFER_SIZE];
    uint16_t rx_write_index;
    uint16_t tx_read_index;
    uint16_t tx_write_index;
    uint32_t last_command_time_ms;
    uint32_t command_count;
    uint32_t error_count;
} DebugManagerState;

// 调试管理器函数声明
void UARTDebug_Init(void);
void UARTDebug_DeInit(void);

void UARTDebug_Start(void);
void UARTDebug_Stop(void);
void UARTDebug_Reset(void);

DebugState UARTDebug_GetState(void);
void UARTDebug_GetStateInfo(DebugManagerState* state);

// 命令接收和处理
void UARTDebug_ProcessReceivedByte(uint8_t byte);
void UARTDebug_ProcessBuffer(void);
bool UARTDebug_HasPendingCommand(void);
DebugCommandParseResult UARTDebug_GetPendingCommand(void);

// 响应发送
void UARTDebug_SendResponse(const char* response);
void UARTDebug_SendStatus(void);
void UARTDebug_SendError(uint8_t error_code, const char* message);
void UARTDebug_SendAck(void);
void UARTDebug_SendNack(void);

// 系统状态报告
void UARTDebug_ReportFrameStatus(const ImageProcessingOutput* output);
void UARTDebug_ReportObstacles(const ObstacleDetectionResult* result);
void UARTDebug_ReportMotion(const MotionState* state);
void UARTDebug_ReportSystemStatus(const SystemStatus* status);

// 命令执行
void UARTDebug_ExecuteCommand(const DebugCommand* command);

// 辅助函数
bool UARTDebug_ParseCommand(const uint8_t* buffer, uint16_t length, DebugCommandParseResult* result);
uint8_t UARTDebug_CalculateChecksum(const uint8_t* data, uint16_t length);

// 周期性处理（应在主循环中调用）
void UARTDebug_Tick(void);

#ifdef __cplusplus
}
#endif

#endif // UART_DEBUG_H
