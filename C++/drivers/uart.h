#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// UART 端口定义
typedef enum {
    UART_PORT_1 = 0,
    UART_PORT_2 = 1,
    UART_PORT_3 = 2,
    UART_PORT_4 = 3,
    UART_PORT_5 = 4,
    UART_PORT_6 = 5,
    UART_PORT_7 = 6,
    UART_PORT_8 = 7
} UARTPort;

// UART 波特率定义
typedef enum {
    UART_BAUD_9600 = 9600,
    UART_BAUD_19200 = 19200,
    UART_BAUD_38400 = 38400,
    UART_BAUD_57600 = 57600,
    UART_BAUD_115200 = 115200,
    UART_BAUD_230400 = 230400,
    UART_BAUD_460800 = 460800,
    UART_BAUD_921600 = 921600
} UARTBaudRate;

// UART 数据位定义
typedef enum {
    UART_DATA_BITS_8 = 0,
    UART_DATA_BITS_9 = 1
} UARTDataBits;

// UART 停止位定义
typedef enum {
    UART_STOP_BITS_1 = 0,
    UART_STOP_BITS_0_5 = 1,
    UART_STOP_BITS_2 = 2,
    UART_STOP_BITS_1_5 = 3
} UARTStopBits;

// UART 校验位定义
typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN = 1,
    UART_PARITY_ODD = 2
} UARTParity;

// UART 流控制定义
typedef enum {
    UART_FLOW_CONTROL_NONE = 0,
    UART_FLOW_CONTROL_RTS = 1,
    UART_FLOW_CONTROL_CTS = 2,
    UART_FLOW_CONTROL_RTS_CTS = 3
} UARTFlowControl;

// UART 配置结构
typedef struct {
    UARTBaudRate baud_rate;
    UARTDataBits data_bits;
    UARTStopBits stop_bits;
    UARTParity parity;
    UARTFlowControl flow_control;
    bool tx_enable;
    bool rx_enable;
} UARTConfig;

// UART 状态结构
typedef struct {
    bool tx_empty;
    bool tx_complete;
    bool rx_not_empty;
    bool overrun_error;
    bool parity_error;
    bool framing_error;
    bool break_detected;
} UARTStatus;

// UART 函数声明
void UART_Init(UARTPort port, const UARTConfig* config);
void UART_DeInit(UARTPort port);

void UART_Enable(UARTPort port);
void UART_Disable(UARTPort port);

bool UART_WriteByte(UARTPort port, uint8_t byte);
bool UART_ReadByte(UARTPort port, uint8_t* byte);

uint16_t UART_Write(UARTPort port, const uint8_t* data, uint16_t length);
uint16_t UART_Read(UARTPort port, uint8_t* data, uint16_t length);

void UART_PrintString(UARTPort port, const char* str);

UARTStatus UART_GetStatus(UARTPort port);
void UART_ClearErrors(UARTPort port);

// 中断相关函数
void UART_EnableTxInterrupt(UARTPort port);
void UART_DisableTxInterrupt(UARTPort port);
void UART_EnableRxInterrupt(UARTPort port);
void UART_DisableRxInterrupt(UARTPort port);

// 调试用的默认 UART 端口
#define DEBUG_UART_PORT UART_PORT_1

// 默认 UART 配置
#define DEFAULT_UART_CONFIG { \
    .baud_rate = UART_BAUD_115200, \
    .data_bits = UART_DATA_BITS_8, \
    .stop_bits = UART_STOP_BITS_1, \
    .parity = UART_PARITY_NONE, \
    .flow_control = UART_FLOW_CONTROL_NONE, \
    .tx_enable = true, \
    .rx_enable = true \
}

#ifdef __cplusplus
}
#endif

#endif // UART_H
