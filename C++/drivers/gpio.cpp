#include "gpio.h"

// STM32H7 寄存器基地址
#define RCC_BASE 0x58024400
#define RCC_AHB4ENR_OFFSET 0xE8

#define GPIOA_BASE 0x58020000
#define GPIOB_BASE 0x58020400
#define GPIOC_BASE 0x58020800
#define GPIOD_BASE 0x58020C00
#define GPIOE_BASE 0x58021000
#define GPIOF_BASE 0x58021400
#define GPIOG_BASE 0x58021800
#define GPIOH_BASE 0x58021C00

// GPIO 寄存器偏移
#define GPIO_MODER_OFFSET 0x00
#define GPIO_OTYPER_OFFSET 0x04
#define GPIO_OSPEEDR_OFFSET 0x08
#define GPIO_PUPDR_OFFSET 0x0C
#define GPIO_IDR_OFFSET 0x10
#define GPIO_ODR_OFFSET 0x14
#define GPIO_BSRR_OFFSET 0x18
#define GPIO_LCKR_OFFSET 0x1C
#define GPIO_AFRL_OFFSET 0x20
#define GPIO_AFRH_OFFSET 0x24

// 寄存器访问宏
#define REG32(addr) (*(volatile uint32_t*)(addr))
#define REG16(addr) (*(volatile uint16_t*)(addr))

// GPIO 端口基地址表
static const uint32_t GPIO_BASES[] = {
    GPIOA_BASE,
    GPIOB_BASE,
    GPIOC_BASE,
    GPIOD_BASE,
    GPIOE_BASE,
    GPIOF_BASE,
    GPIOG_BASE,
    GPIOH_BASE
};

// 使能 GPIO 时钟
static void GPIO_EnableClock(GPIOPort port) {
    uint32_t rcc_ahb4enr = RCC_BASE + RCC_AHB4ENR_OFFSET;
    uint32_t enable_bit = 1UL << static_cast<uint32_t>(port);
    REG32(rcc_ahb4enr) |= enable_bit;
    __asm__ __volatile__("dmb sy" ::: "memory");
}

void GPIO_Init(GPIOPort port, GPIOPin pin, const GPIOConfig* config) {
    if (config == nullptr) {
        return;
    }
    
    GPIO_EnableClock(port);
    
    uint32_t gpio_base = GPIO_BASES[static_cast<uint32_t>(port)];
    uint32_t pin_num = static_cast<uint32_t>(pin);
    
    // 设置模式 (MODER)
    uint32_t moder = REG32(gpio_base + GPIO_MODER_OFFSET);
    moder &= ~(3UL << (pin_num * 2));
    moder |= static_cast<uint32_t>(config->mode) << (pin_num * 2);
    REG32(gpio_base + GPIO_MODER_OFFSET) = moder;
    
    // 设置输出类型 (OTYPER)
    uint32_t otyper = REG32(gpio_base + GPIO_OTYPER_OFFSET);
    otyper &= ~(1UL << pin_num);
    otyper |= static_cast<uint32_t>(config->output_type) << pin_num;
    REG32(gpio_base + GPIO_OTYPER_OFFSET) = otyper;
    
    // 设置速度 (OSPEEDR)
    uint32_t ospeedr = REG32(gpio_base + GPIO_OSPEEDR_OFFSET);
    ospeedr &= ~(3UL << (pin_num * 2));
    ospeedr |= static_cast<uint32_t>(config->speed) << (pin_num * 2);
    REG32(gpio_base + GPIO_OSPEEDR_OFFSET) = ospeedr;
    
    // 设置上拉/下拉 (PUPDR)
    uint32_t pupdr = REG32(gpio_base + GPIO_PUPDR_OFFSET);
    pupdr &= ~(3UL << (pin_num * 2));
    pupdr |= static_cast<uint32_t>(config->pull_up_down) << (pin_num * 2);
    REG32(gpio_base + GPIO_PUPDR_OFFSET) = pupdr;
    
    // 设置复用功能 (AFR)
    if (config->mode == GPIO_MODE_AF) {
        uint32_t afr_offset;
        uint32_t afr_shift;
        
        if (pin_num < 8) {
            afr_offset = GPIO_AFRL_OFFSET;
            afr_shift = pin_num * 4;
        } else {
            afr_offset = GPIO_AFRH_OFFSET;
            afr_shift = (pin_num - 8) * 4;
        }
        
        uint32_t afr = REG32(gpio_base + afr_offset);
        afr &= ~(15UL << afr_shift);
        afr |= static_cast<uint32_t>(config->alternate_function) << afr_shift;
        REG32(gpio_base + afr_offset) = afr;
    }
}

void GPIO_DeInit(GPIOPort port, GPIOPin pin) {
    uint32_t gpio_base = GPIO_BASES[static_cast<uint32_t>(port)];
    uint32_t pin_num = static_cast<uint32_t>(pin);
    
    // 设置为输入模式
    uint32_t moder = REG32(gpio_base + GPIO_MODER_OFFSET);
    moder &= ~(3UL << (pin_num * 2));
    moder |= static_cast<uint32_t>(GPIO_MODE_INPUT) << (pin_num * 2);
    REG32(gpio_base + GPIO_MODER_OFFSET) = moder;
    
    // 清除输出类型
    uint32_t otyper = REG32(gpio_base + GPIO_OTYPER_OFFSET);
    otyper &= ~(1UL << pin_num);
    REG32(gpio_base + GPIO_OTYPER_OFFSET) = otyper;
    
    // 设置低速
    uint32_t ospeedr = REG32(gpio_base + GPIO_OSPEEDR_OFFSET);
    ospeedr &= ~(3UL << (pin_num * 2));
    ospeedr |= static_cast<uint32_t>(GPIO_SPEED_LOW) << (pin_num * 2);
    REG32(gpio_base + GPIO_OSPEEDR_OFFSET) = ospeedr;
    
    // 无上拉下拉
    uint32_t pupdr = REG32(gpio_base + GPIO_PUPDR_OFFSET);
    pupdr &= ~(3UL << (pin_num * 2));
    pupdr |= static_cast<uint32_t>(GPIO_PUPD_NO_PULL) << (pin_num * 2);
    REG32(gpio_base + GPIO_PUPDR_OFFSET) = pupdr;
}

void GPIO_WritePin(GPIOPort port, GPIOPin pin, bool state) {
    uint32_t gpio_base = GPIO_BASES[static_cast<uint32_t>(port)];
    uint32_t pin_num = static_cast<uint32_t>(pin);
    
    if (state) {
        // 设置引脚 (BSRR 低 16 位)
        REG32(gpio_base + GPIO_BSRR_OFFSET) = (1UL << pin_num);
    } else {
        // 重置引脚 (BSRR 高 16 位)
        REG32(gpio_base + GPIO_BSRR_OFFSET) = (1UL << (pin_num + 16));
    }
}

bool GPIO_ReadPin(GPIOPort port, GPIOPin pin) {
    uint32_t gpio_base = GPIO_BASES[static_cast<uint32_t>(port)];
    uint32_t pin_num = static_cast<uint32_t>(pin);
    
    uint32_t idr = REG32(gpio_base + GPIO_IDR_OFFSET);
    return (idr & (1UL << pin_num)) != 0;
}

void GPIO_TogglePin(GPIOPort port, GPIOPin pin) {
    uint32_t gpio_base = GPIO_BASES[static_cast<uint32_t>(port)];
    uint32_t pin_num = static_cast<uint32_t>(pin);
    
    uint32_t odr = REG32(gpio_base + GPIO_ODR_OFFSET);
    REG32(gpio_base + GPIO_ODR_OFFSET) = odr ^ (1UL << pin_num);
}

void GPIO_WritePort(GPIOPort port, uint16_t value) {
    uint32_t gpio_base = GPIO_BASES[static_cast<uint32_t>(port)];
    REG32(gpio_base + GPIO_ODR_OFFSET) = static_cast<uint32_t>(value);
}

uint16_t GPIO_ReadPort(GPIOPort port) {
    uint32_t gpio_base = GPIO_BASES[static_cast<uint32_t>(port)];
    return static_cast<uint16_t>(REG32(gpio_base + GPIO_IDR_OFFSET));
}
