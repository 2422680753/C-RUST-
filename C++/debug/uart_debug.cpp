#include "uart_debug.h"
#include "../drivers/uart.h"
#include "../comm/comm_manager.h"
#include "../motor/motor_control.h"
#include <cstring>
#include <cstdio>

// 调试管理器状态
static DebugManagerState g_debug_state = {
    .current_state = DEBUG_STATE_IDLE,
    .rx_buffer = {0},
    .tx_buffer = {0},
    .rx_write_index = 0,
    .tx_read_index = 0,
    .tx_write_index = 0,
    .last_command_time_ms = 0,
    .command_count = 0,
    .error_count = 0
};

// 待处理命令
static DebugCommand g_pending_command;
static bool g_has_pending_command = false;

// 获取当前时间戳（毫秒）
static uint32_t GetCurrentTimeMs(void) {
    // 这里应该使用系统定时器
    return 0;
}

void UARTDebug_Init(void) {
    // 初始化 UART
    UARTConfig uart_config = DEFAULT_UART_CONFIG;
    UART_Init(DEBUG_UART_PORT, &uart_config);
    
    // 启用接收中断
    UART_EnableRxInterrupt(DEBUG_UART_PORT);
    
    // 重置状态
    g_debug_state.current_state = DEBUG_STATE_IDLE;
    g_debug_state.rx_write_index = 0;
    g_debug_state.tx_read_index = 0;
    g_debug_state.tx_write_index = 0;
    g_debug_state.last_command_time_ms = 0;
    g_debug_state.command_count = 0;
    g_debug_state.error_count = 0;
    
    g_has_pending_command = false;
    
    // 清空缓冲区
    std::memset(g_debug_state.rx_buffer, 0, DEBUG_COMMAND_BUFFER_SIZE);
    std::memset(g_debug_state.tx_buffer, 0, DEBUG_RESPONSE_BUFFER_SIZE);
}

void UARTDebug_DeInit(void) {
    UART_DisableRxInterrupt(DEBUG_UART_PORT);
    UART_DisableTxInterrupt(DEBUG_UART_PORT);
    UART_DeInit(DEBUG_UART_PORT);
    
    g_debug_state.current_state = DEBUG_STATE_IDLE;
    g_has_pending_command = false;
}

void UARTDebug_Start(void) {
    g_debug_state.current_state = DEBUG_STATE_IDLE;
    UART_Enable(DEBUG_UART_PORT);
}

void UARTDebug_Stop(void) {
    g_debug_state.current_state = DEBUG_STATE_IDLE;
    UART_Disable(DEBUG_UART_PORT);
}

void UARTDebug_Reset(void) {
    UARTDebug_DeInit();
    UARTDebug_Init();
}

DebugState UARTDebug_GetState(void) {
    return g_debug_state.current_state;
}

void UARTDebug_GetStateInfo(DebugManagerState* state) {
    if (state == nullptr) {
        return;
    }
    
    *state = g_debug_state;
}

void UARTDebug_ProcessReceivedByte(uint8_t byte) {
    if (g_debug_state.current_state == DEBUG_STATE_IDLE) {
        if (byte == DEBUG_COMMAND_PREFIX) {
            g_debug_state.current_state = DEBUG_STATE_RECEIVING;
            g_debug_state.rx_write_index = 0;
        }
        return;
    }
    
    if (g_debug_state.current_state == DEBUG_STATE_RECEIVING) {
        // 检查是否为命令结束符（换行符）
        if (byte == '\n' || byte == '\r') {
            if (g_debug_state.rx_write_index > 0) {
                g_debug_state.rx_buffer[g_debug_state.rx_write_index] = '\0';
                UARTDebug_ProcessBuffer();
            }
            g_debug_state.current_state = DEBUG_STATE_IDLE;
            g_debug_state.rx_write_index = 0;
            return;
        }
        
        // 检查缓冲区是否已满
        if (g_debug_state.rx_write_index >= DEBUG_COMMAND_BUFFER_SIZE - 1) {
            g_debug_state.error_count++;
            g_debug_state.current_state = DEBUG_STATE_ERROR;
            UARTDebug_SendError(1, "Command buffer overflow");
            g_debug_state.current_state = DEBUG_STATE_IDLE;
            g_debug_state.rx_write_index = 0;
            return;
        }
        
        // 存储字节
        g_debug_state.rx_buffer[g_debug_state.rx_write_index++] = byte;
    }
}

void UARTDebug_ProcessBuffer(void) {
    DebugCommandParseResult result;
    
    if (UARTDebug_ParseCommand(g_debug_state.rx_buffer, g_debug_state.rx_write_index, &result)) {
        if (result.is_valid) {
            g_pending_command = result.command;
            g_has_pending_command = true;
            g_debug_state.current_state = DEBUG_STATE_PROCESSING;
            g_debug_state.command_count++;
            g_debug_state.last_command_time_ms = GetCurrentTimeMs();
            
            UARTDebug_ExecuteCommand(&result.command);
            UARTDebug_SendAck();
            
            g_debug_state.current_state = DEBUG_STATE_IDLE;
        } else {
            g_debug_state.error_count++;
            UARTDebug_SendError(result.error_code, "Invalid command");
        }
    } else {
        g_debug_state.error_count++;
        UARTDebug_SendError(2, "Command parse error");
    }
}

bool UARTDebug_HasPendingCommand(void) {
    return g_has_pending_command;
}

DebugCommandParseResult UARTDebug_GetPendingCommand(void) {
    DebugCommandParseResult result;
    result.is_valid = false;
    result.error_code = 0;
    
    if (g_has_pending_command) {
        result.command = g_pending_command;
        result.is_valid = true;
        g_has_pending_command = false;
    }
    
    return result;
}

void UARTDebug_SendResponse(const char* response) {
    if (response == nullptr) {
        return;
    }
    
    // 计算响应长度
    uint16_t response_len = 0;
    const char* ptr = response;
    while (*ptr != '\0' && response_len < DEBUG_RESPONSE_BUFFER_SIZE - 2) {
        response_len++;
        ptr++;
    }
    
    // 检查 TX 缓冲区空间
    uint16_t available_space;
    if (g_debug_state.tx_write_index >= g_debug_state.tx_read_index) {
        available_space = DEBUG_RESPONSE_BUFFER_SIZE - g_debug_state.tx_write_index + g_debug_state.tx_read_index - 1;
    } else {
        available_space = g_debug_state.tx_read_index - g_debug_state.tx_write_index - 1;
    }
    
    if (response_len + 2 > available_space) {
        return;
    }
    
    // 添加响应前缀
    g_debug_state.tx_buffer[g_debug_state.tx_write_index++] = DEBUG_RESPONSE_PREFIX;
    
    // 复制响应内容
    for (uint16_t i = 0; i < response_len; i++) {
        g_debug_state.tx_buffer[g_debug_state.tx_write_index++] = response[i];
        if (g_debug_state.tx_write_index >= DEBUG_RESPONSE_BUFFER_SIZE) {
            g_debug_state.tx_write_index = 0;
        }
    }
    
    // 添加换行符
    g_debug_state.tx_buffer[g_debug_state.tx_write_index++] = '\n';
    
    // 启用发送中断
    UART_EnableTxInterrupt(DEBUG_UART_PORT);
}

void UARTDebug_SendStatus(void) {
    SystemStatus status;
    CommManager_GetSystemStatus(&status);
    
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), 
        "STATUS:FRM=%lu,FPS=%lu,CPU=%u,OBS=%u,DIR=%d",
        status.frame_count,
        status.fps_actual,
        status.average_cpu_usage,
        status.obstacle_count,
        static_cast<int>(status.current_direction)
    );
    
    UARTDebug_SendResponse(buffer);
}

void UARTDebug_SendError(uint8_t error_code, const char* message) {
    if (message == nullptr) {
        message = "Unknown error";
    }
    
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), 
        "ERROR:CODE=%u,MSG=%s",
        error_code,
        message
    );
    
    UARTDebug_SendResponse(buffer);
}

void UARTDebug_SendAck(void) {
    UARTDebug_SendResponse("ACK");
}

void UARTDebug_SendNack(void) {
    UARTDebug_SendResponse("NACK");
}

void UARTDebug_ReportFrameStatus(const ImageProcessingOutput* output) {
    if (output == nullptr || !output->is_valid) {
        return;
    }
    
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), 
        "FRAME:TIME=%lu,CPU=%u,OBS=%u,DIR=%d",
        output->processing_time_ms,
        output->cpu_usage_estimate,
        output->obstacle_result.count,
        static_cast<int>(output->motion_state.direction)
    );
    
    UARTDebug_SendResponse(buffer);
}

void UARTDebug_ReportObstacles(const ObstacleDetectionResult* result) {
    if (result == nullptr) {
        return;
    }
    
    for (uint16_t i = 0; i < result->count; i++) {
        const Obstacle* obs = &result->obstacles[i];
        char buffer[80];
        std::snprintf(buffer, sizeof(buffer), 
            "OBS[%u]:X=%d,Y=%d,W=%d,H=%d,DIST=%d,CONF=%u",
            i,
            obs->center_x,
            obs->center_y,
            obs->width,
            obs->height,
            obs->estimated_distance,
            obs->confidence
        );
        UARTDebug_SendResponse(buffer);
    }
}

void UARTDebug_ReportMotion(const MotionState* state) {
    if (state == nullptr) {
        return;
    }
    
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), 
        "MOTION:DIR=%d,VX=%d,VY=%d,ROT=%d,CONF=%u",
        static_cast<int>(state->direction),
        state->velocity_x,
        state->velocity_y,
        state->rotation,
        state->confidence
    );
    
    UARTDebug_SendResponse(buffer);
}

void UARTDebug_ReportSystemStatus(const SystemStatus* status) {
    if (status == nullptr) {
        return;
    }
    
    char buffer[80];
    std::snprintf(buffer, sizeof(buffer), 
        "SYS:FRM=%lu,FPS=%lu,TIME=%lu,CPU=%u,OBS=%u,DIR=%d,UP=%lu",
        status->frame_count,
        status->fps_actual,
        status->total_processing_time_ms,
        status->average_cpu_usage,
        status->obstacle_count,
        static_cast<int>(status->current_direction),
        status->uptime_ms
    );
    
    UARTDebug_SendResponse(buffer);
}

void UARTDebug_ExecuteCommand(const DebugCommand* command) {
    if (command == nullptr) {
        return;
    }
    
    switch (command->type) {
        case DEBUG_CMD_START:
            CommManager_Start();
            MotorControl_Enable();
            break;
            
        case DEBUG_CMD_STOP:
            CommManager_Stop();
            MotorControl_Stop();
            MotorControl_Disable();
            break;
            
        case DEBUG_CMD_RESET:
            CommManager_Reset();
            MotorControl_Init();
            break;
            
        case DEBUG_CMD_SET_SPEED:
            // param1 = 左速度, param2 = 右速度
            MotorControl_SetSpeed(
                static_cast<int16_t>(command->param1),
                static_cast<int16_t>(command->param2)
            );
            break;
            
        case DEBUG_CMD_GET_STATUS:
            UARTDebug_SendStatus();
            break;
            
        case DEBUG_CMD_SET_PARAM:
            rust_set_parameter(
                static_cast<DebugParameter>(command->param1),
                command->param2
            );
            break;
            
        case DEBUG_CMD_GET_PARAM: {
            int32_t value = rust_get_parameter(
                static_cast<DebugParameter>(command->param1)
            );
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "PARAM:VAL=%ld", value);
            UARTDebug_SendResponse(buffer);
            break;
        }
            
        default:
            UARTDebug_SendNack();
            break;
    }
}

bool UARTDebug_ParseCommand(const uint8_t* buffer, uint16_t length, DebugCommandParseResult* result) {
    if (buffer == nullptr || length == 0 || result == nullptr) {
        return false;
    }
    
    result->is_valid = false;
    result->error_code = 0;
    
    // 跳过命令前缀（如果有）
    uint16_t start_index = 0;
    if (buffer[0] == DEBUG_COMMAND_PREFIX) {
        start_index = 1;
    }
    
    // 解析命令类型
    // 格式: #CMD:P1,P2,C
    // 例如: #START, #STOP, #SETPARAM:0,20,0
    
    uint16_t cmd_end = start_index;
    while (cmd_end < length && buffer[cmd_end] != ':' && buffer[cmd_end] != ',') {
        cmd_end++;
    }
    
    // 比较命令字符串
    const uint8_t* cmd_ptr = buffer + start_index;
    uint16_t cmd_len = cmd_end - start_index;
    
    if (cmd_len == 5 && std::memcmp(cmd_ptr, "START", 5) == 0) {
        result->command.type = DEBUG_CMD_START;
        result->command.param1 = 0;
        result->command.param2 = 0;
        result->is_valid = true;
    } else if (cmd_len == 4 && std::memcmp(cmd_ptr, "STOP", 4) == 0) {
        result->command.type = DEBUG_CMD_STOP;
        result->is_valid = true;
    } else if (cmd_len == 5 && std::memcmp(cmd_ptr, "RESET", 5) == 0) {
        result->command.type = DEBUG_CMD_RESET;
        result->is_valid = true;
    } else if (cmd_len == 8 && std::memcmp(cmd_ptr, "SETSPEED", 8) == 0) {
        result->command.type = DEBUG_CMD_SET_SPEED;
        // 解析参数
        if (cmd_end < length && buffer[cmd_end] == ':') {
            // 简单解析: SEETSPEED:L,R
            uint16_t p1_start = cmd_end + 1;
            uint16_t p1_end = p1_start;
            while (p1_end < length && buffer[p1_end] != ',') {
                p1_end++;
            }
            if (p1_end < length) {
                // 解析 P1
                int32_t p1 = 0;
                for (uint16_t i = p1_start; i < p1_end; i++) {
                    if (buffer[i] >= '0' && buffer[i] <= '9') {
                        p1 = p1 * 10 + (buffer[i] - '0');
                    }
                }
                result->command.param1 = static_cast<uint32_t>(p1);
                
                // 解析 P2
                uint16_t p2_start = p1_end + 1;
                uint16_t p2_end = p2_start;
                while (p2_end < length && buffer[p2_end] != ',' && buffer[p2_end] != '\0') {
                    p2_end++;
                }
                int32_t p2 = 0;
                for (uint16_t i = p2_start; i < p2_end; i++) {
                    if (buffer[i] >= '0' && buffer[i] <= '9') {
                        p2 = p2 * 10 + (buffer[i] - '0');
                    }
                }
                result->command.param2 = p2;
                result->is_valid = true;
            }
        }
    } else if (cmd_len == 9 && std::memcmp(cmd_ptr, "GETSTATUS", 9) == 0) {
        result->command.type = DEBUG_CMD_GET_STATUS;
        result->is_valid = true;
    } else if (cmd_len == 8 && std::memcmp(cmd_ptr, "SETPARAM", 8) == 0) {
        result->command.type = DEBUG_CMD_SET_PARAM;
        // 类似的参数解析
        result->is_valid = true;
    } else if (cmd_len == 8 && std::memcmp(cmd_ptr, "GETPARAM", 8) == 0) {
        result->command.type = DEBUG_CMD_GET_PARAM;
        result->is_valid = true;
    } else {
        result->error_code = 1;
        return false;
    }
    
    // 计算校验和（简化版）
    result->command.checksum = UARTDebug_CalculateChecksum(buffer, length);
    
    return true;
}

uint8_t UARTDebug_CalculateChecksum(const uint8_t* data, uint16_t length) {
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

void UARTDebug_Tick(void) {
    // 处理接收
    uint8_t byte;
    while (UART_ReadByte(DEBUG_UART_PORT, &byte)) {
        UARTDebug_ProcessReceivedByte(byte);
    }
    
    // 处理发送
    while (g_debug_state.tx_read_index != g_debug_state.tx_write_index) {
        UARTStatus status = UART_GetStatus(DEBUG_UART_PORT);
        if (status.tx_empty) {
            uint8_t tx_byte = g_debug_state.tx_buffer[g_debug_state.tx_read_index];
            UART_WriteByte(DEBUG_UART_PORT, tx_byte);
            
            g_debug_state.tx_read_index++;
            if (g_debug_state.tx_read_index >= DEBUG_RESPONSE_BUFFER_SIZE) {
                g_debug_state.tx_read_index = 0;
            }
        } else {
            break;
        }
    }
    
    // 检查发送完成后禁用中断
    if (g_debug_state.tx_read_index == g_debug_state.tx_write_index) {
        UART_DisableTxInterrupt(DEBUG_UART_PORT);
    }
}
