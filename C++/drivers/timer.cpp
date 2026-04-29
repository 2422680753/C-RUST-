#include "timer.h"

// STM32H7 定时器寄存器基地址
#define TIM1_BASE 0x40010000
#define TIM2_BASE 0x40000000
#define TIM3_BASE 0x40000400
#define TIM4_BASE 0x40000800
#define TIM5_BASE 0x40000C00
#define TIM6_BASE 0x40001000
#define TIM7_BASE 0x40001400
#define TIM8_BASE 0x40010400
#define TIM12_BASE 0x40001800
#define TIM13_BASE 0x40001C00
#define TIM14_BASE 0x40002000
#define TIM15_BASE 0x40014000
#define TIM16_BASE 0x40014400
#define TIM17_BASE 0x40014800

// RCC 寄存器
#define RCC_BASE 0x58024400
#define RCC_APB1LENR_OFFSET 0xE8
#define RCC_APB2ENR_OFFSET 0xE0

// 定时器寄存器偏移
#define TIM_CR1_OFFSET 0x00
#define TIM_CR2_OFFSET 0x04
#define TIM_SMCR_OFFSET 0x08
#define TIM_DIER_OFFSET 0x0C
#define TIM_SR_OFFSET 0x10
#define TIM_EGR_OFFSET 0x14
#define TIM_CCMR1_OFFSET 0x18
#define TIM_CCMR2_OFFSET 0x1C
#define TIM_CCER_OFFSET 0x20
#define TIM_CNT_OFFSET 0x24
#define TIM_PSC_OFFSET 0x28
#define TIM_ARR_OFFSET 0x2C
#define TIM_RCR_OFFSET 0x30
#define TIM_CCR1_OFFSET 0x34
#define TIM_CCR2_OFFSET 0x38
#define TIM_CCR3_OFFSET 0x3C
#define TIM_CCR4_OFFSET 0x40
#define TIM_BDTR_OFFSET 0x44
#define TIM_DCR_OFFSET 0x48
#define TIM_DMAR_OFFSET 0x4C
#define TIM_CCMR3_OFFSET 0x50
#define TIM_CCR5_OFFSET 0x54
#define TIM_CCR6_OFFSET 0x58
#define TIM_AF1_OFFSET 0x60
#define TIM_AF2_OFFSET 0x64
#define TIM_TISEL_OFFSET 0x68

// 定时器 CR1 寄存器位定义
#define TIM_CR1_CEN (1UL << 0)
#define TIM_CR1_UDIS (1UL << 1)
#define TIM_CR1_URS (1UL << 2)
#define TIM_CR1_OPM (1UL << 3)
#define TIM_CR1_DIR (1UL << 4)
#define TIM_CR1_CMS_MASK (3UL << 5)
#define TIM_CR1_ARPE (1UL << 7)
#define TIM_CR1_CKD_MASK (3UL << 8)
#define TIM_CR1_UIFREMAP (1UL << 11)

// 定时器 CCMR1/CCMR2 寄存器位定义
#define TIM_CCMR_CC1S_MASK (3UL << 0)
#define TIM_CCMR_OC1FE (1UL << 2)
#define TIM_CCMR_OC1PE (1UL << 3)
#define TIM_CCMR_OC1M_MASK (7UL << 4)
#define TIM_CCMR_OC1M_3 (1UL << 16)
#define TIM_CCMR_CC2S_MASK (3UL << 8)
#define TIM_CCMR_OC2FE (1UL << 10)
#define TIM_CCMR_OC2PE (1UL << 11)
#define TIM_CCMR_OC2M_MASK (7UL << 12)
#define TIM_CCMR_OC2M_3 (1UL << 24)
#define TIM_CCMR_CC3S_MASK (3UL << 0)
#define TIM_CCMR_OC3FE (1UL << 2)
#define TIM_CCMR_OC3PE (1UL << 3)
#define TIM_CCMR_OC3M_MASK (7UL << 4)
#define TIM_CCMR_OC3M_3 (1UL << 16)
#define TIM_CCMR_CC4S_MASK (3UL << 8)
#define TIM_CCMR_OC4FE (1UL << 10)
#define TIM_CCMR_OC4PE (1UL << 11)
#define TIM_CCMR_OC4M_MASK (7UL << 12)
#define TIM_CCMR_OC4M_3 (1UL << 24)

// 定时器 CCER 寄存器位定义
#define TIM_CCER_CC1E (1UL << 0)
#define TIM_CCER_CC1P (1UL << 1)
#define TIM_CCER_CC1NE (1UL << 2)
#define TIM_CCER_CC1NP (1UL << 3)
#define TIM_CCER_CC2E (1UL << 4)
#define TIM_CCER_CC2P (1UL << 5)
#define TIM_CCER_CC2NE (1UL << 6)
#define TIM_CCER_CC2NP (1UL << 7)
#define TIM_CCER_CC3E (1UL << 8)
#define TIM_CCER_CC3P (1UL << 9)
#define TIM_CCER_CC3NE (1UL << 10)
#define TIM_CCER_CC3NP (1UL << 11)
#define TIM_CCER_CC4E (1UL << 12)
#define TIM_CCER_CC4P (1UL << 13)

// 定时器 BDTR 寄存器位定义
#define TIM_BDTR_DTG_MASK (0xFFUL << 0)
#define TIM_BDTR_LOCK_MASK (3UL << 8)
#define TIM_BDTR_OSSI (1UL << 10)
#define TIM_BDTR_OSSR (1UL << 11)
#define TIM_BDTR_BKE (1UL << 12)
#define TIM_BDTR_BKP (1UL << 13)
#define TIM_BDTR_AOE (1UL << 14)
#define TIM_BDTR_MOE (1UL << 15)
#define TIM_BDTR_BKF_MASK (0xFUL << 16)
#define TIM_BDTR_BK2F_MASK (0xFUL << 20)
#define TIM_BDTR_BK2E (1UL << 24)
#define TIM_BDTR_BK2P (1UL << 25)

// 定时器 SR 寄存器位定义
#define TIM_SR_UIF (1UL << 0)
#define TIM_SR_CC1IF (1UL << 1)
#define TIM_SR_CC2IF (1UL << 2)
#define TIM_SR_CC3IF (1UL << 3)
#define TIM_SR_CC4IF (1UL << 4)
#define TIM_SR_COMIF (1UL << 5)
#define TIM_SR_TIF (1UL << 6)
#define TIM_SR_BIF (1UL << 7)
#define TIM_SR_B2IF (1UL << 8)
#define TIM_SR_CC1OF (1UL << 9)
#define TIM_SR_CC2OF (1UL << 10)
#define TIM_SR_CC3OF (1UL << 11)
#define TIM_SR_CC4OF (1UL << 12)
#define TIM_SR_SBIF (1UL << 13)
#define TIM_SR_CC5IF (1UL << 16)
#define TIM_SR_CC6IF (1UL << 17)

// 定时器 EGR 寄存器位定义
#define TIM_EGR_UG (1UL << 0)
#define TIM_EGR_CC1G (1UL << 1)
#define TIM_EGR_CC2G (1UL << 2)
#define TIM_EGR_CC3G (1UL << 3)
#define TIM_EGR_CC4G (1UL << 4)
#define TIM_EGR_COMG (1UL << 5)
#define TIM_EGR_TG (1UL << 6)
#define TIM_EGR_BG (1UL << 7)
#define TIM_EGR_B2G (1UL << 8)

// 系统时钟频率 (STM32H7 通常为 400MHz)
#define SYSTEM_CLOCK_FREQ 400000000UL

// 寄存器访问宏
#define REG32(addr) (*(volatile uint32_t*)(addr))

// 定时器端口基地址表
static const uint32_t TIMER_BASES[] = {
    TIM1_BASE,
    TIM2_BASE,
    TIM3_BASE,
    TIM4_BASE,
    TIM5_BASE,
    TIM6_BASE,
    TIM7_BASE,
    TIM8_BASE,
    TIM12_BASE,
    TIM13_BASE,
    TIM14_BASE,
    TIM15_BASE,
    TIM16_BASE,
    TIM17_BASE
};

// 使能定时器时钟
static void Timer_EnableClock(TimerPort timer) {
    uint32_t rcc_enr;
    uint32_t enable_bit;
    
    switch (timer) {
        case TIMER_1:
        case TIMER_8:
            rcc_enr = RCC_BASE + RCC_APB2ENR_OFFSET;
            enable_bit = (timer == TIMER_1) ? (1UL << 0) : (1UL << 1);
            break;
        case TIMER_2:
        case TIMER_3:
        case TIMER_4:
        case TIMER_5:
        case TIMER_6:
        case TIMER_7:
        case TIMER_12:
        case TIMER_13:
        case TIMER_14:
            rcc_enr = RCC_BASE + RCC_APB1LENR_OFFSET;
            switch (timer) {
                case TIMER_2: enable_bit = 1UL << 0; break;
                case TIMER_3: enable_bit = 1UL << 1; break;
                case TIMER_4: enable_bit = 1UL << 2; break;
                case TIMER_5: enable_bit = 1UL << 3; break;
                case TIMER_6: enable_bit = 1UL << 4; break;
                case TIMER_7: enable_bit = 1UL << 5; break;
                case TIMER_12: enable_bit = 1UL << 6; break;
                case TIMER_13: enable_bit = 1UL << 7; break;
                case TIMER_14: enable_bit = 1UL << 8; break;
                default: return;
            }
            break;
        case TIMER_15:
        case TIMER_16:
        case TIMER_17:
            rcc_enr = RCC_BASE + RCC_APB2ENR_OFFSET;
            switch (timer) {
                case TIMER_15: enable_bit = 1UL << 16; break;
                case TIMER_16: enable_bit = 1UL << 17; break;
                case TIMER_17: enable_bit = 1UL << 18; break;
                default: return;
            }
            break;
        default:
            return;
    }
    
    REG32(rcc_enr) |= enable_bit;
    __asm__ __volatile__("dmb sy" ::: "memory");
}

void Timer_Init(TimerPort timer, const TimerConfig* config) {
    if (config == nullptr) {
        return;
    }
    
    Timer_EnableClock(timer);
    
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    
    // 禁用定时器
    REG32(timer_base + TIM_CR1_OFFSET) &= ~TIM_CR1_CEN;
    __asm__ __volatile__("dmb sy" ::: "memory");
    
    // 配置 CR1
    uint32_t cr1 = 0;
    
    // 计数模式
    switch (config->count_mode) {
        case TIMER_COUNT_UP:
            cr1 &= ~(TIM_CR1_DIR | TIM_CR1_CMS_MASK);
            break;
        case TIMER_COUNT_DOWN:
            cr1 |= TIM_CR1_DIR;
            cr1 &= ~TIM_CR1_CMS_MASK;
            break;
        case TIMER_COUNT_CENTER_ALIGNED_1:
            cr1 &= ~TIM_CR1_DIR;
            cr1 |= (1UL << 5);
            break;
        case TIMER_COUNT_CENTER_ALIGNED_2:
            cr1 &= ~TIM_CR1_DIR;
            cr1 |= (2UL << 5);
            break;
        case TIMER_COUNT_CENTER_ALIGNED_3:
            cr1 &= ~TIM_CR1_DIR;
            cr1 |= (3UL << 5);
            break;
    }
    
    // 自动重装载预装载
    if (config->auto_reload_preload) {
        cr1 |= TIM_CR1_ARPE;
    } else {
        cr1 &= ~TIM_CR1_ARPE;
    }
    
    // 单脉冲模式
    if (config->one_pulse_mode) {
        cr1 |= TIM_CR1_OPM;
    } else {
        cr1 &= ~TIM_CR1_OPM;
    }
    
    REG32(timer_base + TIM_CR1_OFFSET) = cr1;
    
    // 设置预分频器
    REG32(timer_base + TIM_PSC_OFFSET) = config->prescaler;
    
    // 设置周期
    REG32(timer_base + TIM_ARR_OFFSET) = config->period;
    
    // 生成更新事件以加载寄存器
    REG32(timer_base + TIM_EGR_OFFSET) |= TIM_EGR_UG;
    __asm__ __volatile__("dmb sy" ::: "memory");
    
    // 清除更新标志
    REG32(timer_base + TIM_SR_OFFSET) &= ~TIM_SR_UIF;
}

void Timer_DeInit(TimerPort timer) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    
    // 禁用定时器
    REG32(timer_base + TIM_CR1_OFFSET) &= ~TIM_CR1_CEN;
    __asm__ __volatile__("dmb sy" ::: "memory");
    
    // 重置关键寄存器
    REG32(timer_base + TIM_CR1_OFFSET) = 0;
    REG32(timer_base + TIM_CR2_OFFSET) = 0;
    REG32(timer_base + TIM_SMCR_OFFSET) = 0;
    REG32(timer_base + TIM_DIER_OFFSET) = 0;
    REG32(timer_base + TIM_SR_OFFSET) = 0;
    REG32(timer_base + TIM_EGR_OFFSET) = 0;
    REG32(timer_base + TIM_CCMR1_OFFSET) = 0;
    REG32(timer_base + TIM_CCMR2_OFFSET) = 0;
    REG32(timer_base + TIM_CCER_OFFSET) = 0;
    REG32(timer_base + TIM_CNT_OFFSET) = 0;
    REG32(timer_base + TIM_PSC_OFFSET) = 0;
    REG32(timer_base + TIM_ARR_OFFSET) = 0;
}

void Timer_Start(TimerPort timer) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    REG32(timer_base + TIM_CR1_OFFSET) |= TIM_CR1_CEN;
    __asm__ __volatile__("dmb sy" ::: "memory");
}

void Timer_Stop(TimerPort timer) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    REG32(timer_base + TIM_CR1_OFFSET) &= ~TIM_CR1_CEN;
    __asm__ __volatile__("dmb sy" ::: "memory");
}

void Timer_Reset(TimerPort timer) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    REG32(timer_base + TIM_CNT_OFFSET) = 0;
    REG32(timer_base + TIM_SR_OFFSET) = 0;
}

void Timer_SetPrescaler(TimerPort timer, uint32_t prescaler) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    REG32(timer_base + TIM_PSC_OFFSET) = prescaler;
}

void Timer_SetPeriod(TimerPort timer, uint32_t period) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    REG32(timer_base + TIM_ARR_OFFSET) = period;
}

uint32_t Timer_GetCounter(TimerPort timer) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    return REG32(timer_base + TIM_CNT_OFFSET);
}

void Timer_SetCounter(TimerPort timer, uint32_t value) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    REG32(timer_base + TIM_CNT_OFFSET) = value;
}

bool Timer_IsUpdateEvent(TimerPort timer) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    return (REG32(timer_base + TIM_SR_OFFSET) & TIM_SR_UIF) != 0;
}

void Timer_ClearUpdateEvent(TimerPort timer) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    REG32(timer_base + TIM_SR_OFFSET) &= ~TIM_SR_UIF;
}

// PWM 实现
void PWM_Init(TimerPort timer, TimerChannel channel, const PWMConfig* config) {
    if (config == nullptr) {
        return;
    }
    
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    uint32_t channel_num = static_cast<uint32_t>(channel);
    
    // 配置 CCMR 寄存器
    uint32_t ccmr_offset;
    uint32_t ccmr_shift;
    
    if (channel_num < 2) {
        ccmr_offset = TIM_CCMR1_OFFSET;
        ccmr_shift = channel_num * 8;
    } else {
        ccmr_offset = TIM_CCMR2_OFFSET;
        ccmr_shift = (channel_num - 2) * 8;
    }
    
    uint32_t ccmr = REG32(timer_base + ccmr_offset);
    
    // 清除相关位
    ccmr &= ~(0xFFUL << ccmr_shift);
    
    // 设置为输出模式
    ccmr &= ~(TIM_CCMR_CC1S_MASK << ccmr_shift);
    
    // 设置 PWM 模式
    uint32_t pwm_mode = static_cast<uint32_t>(config->mode);
    if (pwm_mode >= 6) {
        ccmr |= ((pwm_mode & 0x07) << (ccmr_shift + 4));
        if (pwm_mode & 0x08) {
            ccmr |= (1UL << (ccmr_shift + 16));
        }
    } else {
        ccmr |= (pwm_mode << (ccmr_shift + 4));
    }
    
    // 预装载使能
    if (config->preload_enable) {
        ccmr |= (TIM_CCMR_OC1PE << ccmr_shift);
    } else {
        ccmr &= ~(TIM_CCMR_OC1PE << ccmr_shift);
    }
    
    // 快速使能
    if (config->fast_enable) {
        ccmr |= (TIM_CCMR_OC1FE << ccmr_shift);
    } else {
        ccmr &= ~(TIM_CCMR_OC1FE << ccmr_shift);
    }
    
    REG32(timer_base + ccmr_offset) = ccmr;
    
    // 设置脉冲值
    uint32_t ccr_offset;
    switch (channel) {
        case TIMER_CHANNEL_1: ccr_offset = TIM_CCR1_OFFSET; break;
        case TIMER_CHANNEL_2: ccr_offset = TIM_CCR2_OFFSET; break;
        case TIMER_CHANNEL_3: ccr_offset = TIM_CCR3_OFFSET; break;
        case TIMER_CHANNEL_4: ccr_offset = TIM_CCR4_OFFSET; break;
        default: return;
    }
    REG32(timer_base + ccr_offset) = config->pulse;
    
    // 配置 CCER 寄存器
    uint32_t ccer = REG32(timer_base + TIM_CCER_OFFSET);
    uint32_t ccer_shift = channel_num * 4;
    
    // 清除相关位
    ccer &= ~(0xFUL << ccer_shift);
    
    // 输出极性（默认高电平有效）
    ccer &= ~(TIM_CCER_CC1P << ccer_shift);
    
    // 互补输出极性
    ccer &= ~(TIM_CCER_CC1NP << ccer_shift);
    
    // 输出使能
    if (config->output_enable) {
        ccer |= (TIM_CCER_CC1E << ccer_shift);
    }
    
    // 互补输出使能（仅适用于高级定时器）
    if (config->output_complementary_enable) {
        ccer |= (TIM_CCER_CC1NE << ccer_shift);
    }
    
    REG32(timer_base + TIM_CCER_OFFSET) = ccer;
    
    // 对于高级定时器，需要使能主输出
    if (timer == TIMER_1 || timer == TIMER_8) {
        REG32(timer_base + TIM_BDTR_OFFSET) |= TIM_BDTR_MOE;
    }
}

void PWM_DeInit(TimerPort timer, TimerChannel channel) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    uint32_t channel_num = static_cast<uint32_t>(channel);
    
    // 禁用输出
    uint32_t ccer = REG32(timer_base + TIM_CCER_OFFSET);
    ccer &= ~(0xFUL << (channel_num * 4));
    REG32(timer_base + TIM_CCER_OFFSET) = ccer;
    
    // 重置 CCMR 寄存器
    uint32_t ccmr_offset = (channel_num < 2) ? TIM_CCMR1_OFFSET : TIM_CCMR2_OFFSET;
    uint32_t ccmr_shift = (channel_num % 2) * 8;
    
    uint32_t ccmr = REG32(timer_base + ccmr_offset);
    ccmr &= ~(0xFFUL << ccmr_shift);
    REG32(timer_base + ccmr_offset) = ccmr;
}

void PWM_SetPulse(TimerPort timer, TimerChannel channel, uint32_t pulse) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    uint32_t ccr_offset;
    
    switch (channel) {
        case TIMER_CHANNEL_1: ccr_offset = TIM_CCR1_OFFSET; break;
        case TIMER_CHANNEL_2: ccr_offset = TIM_CCR2_OFFSET; break;
        case TIMER_CHANNEL_3: ccr_offset = TIM_CCR3_OFFSET; break;
        case TIMER_CHANNEL_4: ccr_offset = TIM_CCR4_OFFSET; break;
        default: return;
    }
    
    REG32(timer_base + ccr_offset) = pulse;
}

uint32_t PWM_GetPulse(TimerPort timer, TimerChannel channel) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    uint32_t ccr_offset;
    
    switch (channel) {
        case TIMER_CHANNEL_1: ccr_offset = TIM_CCR1_OFFSET; break;
        case TIMER_CHANNEL_2: ccr_offset = TIM_CCR2_OFFSET; break;
        case TIMER_CHANNEL_3: ccr_offset = TIM_CCR3_OFFSET; break;
        case TIMER_CHANNEL_4: ccr_offset = TIM_CCR4_OFFSET; break;
        default: return 0;
    }
    
    return REG32(timer_base + ccr_offset);
}

void PWM_EnableOutput(TimerPort timer, TimerChannel channel) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    uint32_t channel_num = static_cast<uint32_t>(channel);
    
    uint32_t ccer = REG32(timer_base + TIM_CCER_OFFSET);
    ccer |= (TIM_CCER_CC1E << (channel_num * 4));
    REG32(timer_base + TIM_CCER_OFFSET) = ccer;
}

void PWM_DisableOutput(TimerPort timer, TimerChannel channel) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    uint32_t channel_num = static_cast<uint32_t>(channel);
    
    uint32_t ccer = REG32(timer_base + TIM_CCER_OFFSET);
    ccer &= ~(TIM_CCER_CC1E << (channel_num * 4));
    REG32(timer_base + TIM_CCER_OFFSET) = ccer;
}

void PWM_EnableComplementaryOutput(TimerPort timer, TimerChannel channel) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    uint32_t channel_num = static_cast<uint32_t>(channel);
    
    uint32_t ccer = REG32(timer_base + TIM_CCER_OFFSET);
    ccer |= (TIM_CCER_CC1NE << (channel_num * 4));
    REG32(timer_base + TIM_CCER_OFFSET) = ccer;
}

void PWM_DisableComplementaryOutput(TimerPort timer, TimerChannel channel) {
    uint32_t timer_base = TIMER_BASES[static_cast<uint32_t>(timer)];
    uint32_t channel_num = static_cast<uint32_t>(channel);
    
    uint32_t ccer = REG32(timer_base + TIM_CCER_OFFSET);
    ccer &= ~(TIM_CCER_CC1NE << (channel_num * 4));
    REG32(timer_base + TIM_CCER_OFFSET) = ccer;
}
