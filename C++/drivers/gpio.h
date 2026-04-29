#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPIO 端口定义
typedef enum {
    GPIO_PORT_A = 0,
    GPIO_PORT_B = 1,
    GPIO_PORT_C = 2,
    GPIO_PORT_D = 3,
    GPIO_PORT_E = 4,
    GPIO_PORT_F = 5,
    GPIO_PORT_G = 6,
    GPIO_PORT_H = 7
} GPIOPort;

// GPIO 引脚定义
typedef enum {
    GPIO_PIN_0 = 0,
    GPIO_PIN_1 = 1,
    GPIO_PIN_2 = 2,
    GPIO_PIN_3 = 3,
    GPIO_PIN_4 = 4,
    GPIO_PIN_5 = 5,
    GPIO_PIN_6 = 6,
    GPIO_PIN_7 = 7,
    GPIO_PIN_8 = 8,
    GPIO_PIN_9 = 9,
    GPIO_PIN_10 = 10,
    GPIO_PIN_11 = 11,
    GPIO_PIN_12 = 12,
    GPIO_PIN_13 = 13,
    GPIO_PIN_14 = 14,
    GPIO_PIN_15 = 15
} GPIOPin;

// GPIO 模式定义
typedef enum {
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT = 1,
    GPIO_MODE_AF = 2,
    GPIO_MODE_ANALOG = 3
} GPIOMode;

// GPIO 输出类型定义
typedef enum {
    GPIO_OUTPUT_TYPE_PUSH_PULL = 0,
    GPIO_OUTPUT_TYPE_OPEN_DRAIN = 1
} GPIOOutputType;

// GPIO 速度定义
typedef enum {
    GPIO_SPEED_LOW = 0,
    GPIO_SPEED_MEDIUM = 1,
    GPIO_SPEED_HIGH = 2,
    GPIO_SPEED_VERY_HIGH = 3
} GPIOSpeed;

// GPIO 上拉/下拉定义
typedef enum {
    GPIO_PUPD_NO_PULL = 0,
    GPIO_PUPD_PULL_UP = 1,
    GPIO_PUPD_PULL_DOWN = 2
} GPIOPuPd;

// GPIO 配置结构
typedef struct {
    GPIOMode mode;
    GPIOOutputType output_type;
    GPIOSpeed speed;
    GPIOPuPd pull_up_down;
    uint8_t alternate_function;
} GPIOConfig;

// GPIO 函数声明
void GPIO_Init(GPIOPort port, GPIOPin pin, const GPIOConfig* config);
void GPIO_DeInit(GPIOPort port, GPIOPin pin);

void GPIO_WritePin(GPIOPort port, GPIOPin pin, bool state);
bool GPIO_ReadPin(GPIOPort port, GPIOPin pin);

void GPIO_TogglePin(GPIOPort port, GPIOPin pin);

void GPIO_WritePort(GPIOPort port, uint16_t value);
uint16_t GPIO_ReadPort(GPIOPort port);

// 特定引脚的快捷定义
#define MOTOR_LEFT_PWM_PIN GPIO_PIN_0
#define MOTOR_LEFT_PWM_PORT GPIO_PORT_A

#define MOTOR_RIGHT_PWM_PIN GPIO_PIN_1
#define MOTOR_RIGHT_PWM_PORT GPIO_PORT_A

#define MOTOR_LEFT_DIR_PIN GPIO_PIN_2
#define MOTOR_LEFT_DIR_PORT GPIO_PORT_A

#define MOTOR_RIGHT_DIR_PIN GPIO_PIN_3
#define MOTOR_RIGHT_DIR_PORT GPIO_PORT_A

#define UART_TX_PIN GPIO_PIN_9
#define UART_TX_PORT GPIO_PORT_A

#define UART_RX_PIN GPIO_PIN_10
#define UART_RX_PORT GPIO_PORT_A

#define CAMERA_RESET_PIN GPIO_PIN_4
#define CAMERA_RESET_PORT GPIO_PORT_B

#define CAMERA_POWER_PIN GPIO_PIN_5
#define CAMERA_POWER_PORT GPIO_PORT_B

#define LED_STATUS_PIN GPIO_PIN_13
#define LED_STATUS_PORT GPIO_PORT_C

#ifdef __cplusplus
}
#endif

#endif // GPIO_H
