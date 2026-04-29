#include "uart.h"
#include <cstring>

// STM32H7 UART 寄存器基地址
#define USART1_BASE 0x40011000
#define USART2_BASE 0x40004400
#define USART3_BASE 0x40004800
#define UART4_BASE 0x40004C00
#define UART5_BASE 0x40005000
#define USART6_BASE 0x40011400
#define UART7_BASE 0x40007800
#define UART8_BASE 0x40007C00

// RCC 寄存器
#define RCC_BASE 0x58024400
#define RCC_APB1LENR_OFFSET 0xE8
#define RCC_APB2ENR_OFFSET 0xE0

// UART 寄存器偏移
#define USART_CR1_OFFSET 0x00
#define USART_CR2_OFFSET 0x04
#define USART_CR3_OFFSET 0x08
#define USART_BRR_OFFSET 0x0C
#define USART_GTPR_OFFSET 0x10
#define USART_RTOR_OFFSET 0x14
#define USART_RQR_OFFSET 0x18
#define USART_ISR_OFFSET 0x1C
#define USART_ICR_OFFSET 0x20
#define USART_RDR_OFFSET 0x24
#define USART_TDR_OFFSET 0x28

// UART CR1 寄存器位定义
#define USART_CR1_UE (1UL << 0)
#define USART_CR1_UESM (1UL << 1)
#define USART_CR1_RE (1UL << 2)
#define USART_CR1_TE (1UL << 3)
#define USART_CR1_IDLEIE (1UL << 4)
#define USART_CR1_RXNEIE (1UL << 5)
#define USART_CR1_TCIE (1UL << 6)
#define USART_CR1_TXEIE (1UL << 7)
#define USART_CR1_PEIE (1UL << 8)
#define USART_CR1_PS (1UL << 9)
#define USART_CR1_PCE (1UL << 10)
#define USART_CR1_WAKE (1UL << 11)
#define USART_CR1_M0 (1UL << 12)
#define USART_CR1_MME (1UL << 13)
#define USART_CR1_CMIE (1UL << 14)
#define USART_CR1_OVER8 (1UL << 15)
#define USART_CR1_DEDT_MASK (0x1FUL << 16)
#define USART_CR1_DEAT_MASK (0x1FUL << 21)
#define USART_CR1_RTOIE (1UL << 26)
#define USART_CR1_EOBIE (1UL << 27)
#define USART_CR1_M1 (1UL << 28)
#define USART_CR1_FIFOEN (1UL << 29)
#define USART_CR1_TXFEIE (1UL << 30)
#define USART_CR1_RXFFIE (1UL << 31)

// UART CR2 寄存器位定义
#define USART_CR2_SLVEN (1UL << 0)
#define USART_CR2_DIS_NSS (1UL << 3)
#define USART_CR2_ADDM7 (1UL << 4)
#define USART_CR2_LBDL (1UL << 5)
#define USART_CR2_LBDIE (1UL << 6)
#define USART_CR2_LBCL (1UL << 8)
#define USART_CR2_CPHA (1UL << 9)
#define USART_CR2_CPOL (1UL << 10)
#define USART_CR2_CLKEN (1UL << 11)
#define USART_CR2_STOP_MASK (3UL << 12)
#define USART_CR2_LINEN (1UL << 14)
#define USART_CR2_SWAP (1UL << 15)
#define USART_CR2_RXINV (1UL << 16)
#define USART_CR2_TXINV (1UL << 17)
#define USART_CR2_DATAINV (1UL << 18)
#define USART_CR2_MSBFIRST (1UL << 19)
#define USART_CR2_ABREN (1UL << 20)
#define USART_CR2_ABRMODE_MASK (3UL << 21)
#define USART_CR2_RTOEN (1UL << 23)
#define USART_CR2_ADD_MASK (0xFFUL << 24)

// UART CR3 寄存器位定义
#define USART_CR3_EIE (1UL << 0)
#define USART_CR3_IREN (1UL << 1)
#define USART_CR3_IRLP (1UL << 2)
#define USART_CR3_HDSEL (1UL << 3)
#define USART_CR3_NACK (1UL << 4)
#define USART_CR3_SCEN (1UL << 5)
#define USART_CR3_DMAR (1UL << 6)
#define USART_CR3_DMAT (1UL << 7)
#define USART_CR3_RTSE (1UL << 8)
#define USART_CR3_CTSE (1UL << 9)
#define USART_CR3_CTSIE (1UL << 10)
#define USART_CR3_ONEBIT (1UL << 11)
#define USART_CR3_OVRDIS (1UL << 12)
#define USART_CR3_DDRE (1UL << 13)
#define USART_CR3_DEM (1UL << 14)
#define USART_CR3_DEP (1UL << 15)
#define USART_CR3_SCARCNT_MASK (7UL << 17)
#define USART_CR3_WUS_MASK (3UL << 20)
#define USART_CR3_WUFIE (1UL << 22)
#define USART_CR3_TXFTIE (1UL << 23)
#define USART_CR3_TCBGTIE (1UL << 24)
#define USART_CR3_RXFTCFG_MASK (7UL << 25)
#define USART_CR3_RXFTIE (1UL << 28)
#define USART_CR3_TXFTCFG_MASK (7UL << 29)

// UART ISR 寄存器位定义
#define USART_ISR_PE (1UL << 0)
#define USART_ISR_FE (1UL << 1)
#define USART_ISR_NE (1UL << 2)
#define USART_ISR_ORE (1UL << 3)
#define USART_ISR_IDLE (1UL << 4)
#define USART_ISR_RXNE (1UL << 5)
#define USART_ISR_TC (1UL << 6)
#define USART_ISR_TXE (1UL << 7)
#define USART_ISR_LBDF (1UL << 8)
#define USART_ISR_CTSIF (1UL << 9)
#define USART_ISR_CTS (1UL << 10)
#define USART_ISR_RTOF (1UL << 11)
#define USART_ISR_EOBF (1UL << 12)
#define USART_ISR_UDR (1UL << 13)
#define USART_ISR_ABRE (1UL << 14)
#define USART_ISR_ABRF (1UL << 15)
#define USART_ISR_BUSY (1UL << 16)
#define USART_ISR_CMF (1UL << 17)
#define USART_ISR_SBKF (1UL << 18)
#define USART_ISR_RWU (1UL << 19)
#define USART_ISR_WUF (1UL << 20)
#define USART_ISR_TEACK (1UL << 21)
#define USART_ISR_REACK (1UL << 22)
#define USART_ISR_TXFE (1UL << 23)
#define USART_ISR_RXFF (1UL << 24)
#define USART_ISR_TCBGT (1UL << 25)
#define USART_ISR_RXFT (1UL << 26)
#define USART_ISR_TXFT (1UL << 27)

// 寄存器访问宏
#define REG32(addr) (*(volatile uint32_t*)(addr))

// UART 端口基地址表
static const uint32_t UART_BASES[] = {
    USART1_BASE,
    USART2_BASE,
    USART3_BASE,
    UART4_BASE,
    UART5_BASE,
    USART6_BASE,
    UART7_BASE,
    UART8_BASE
};

// 系统时钟频率 (STM32H7 通常为 400MHz)
#define SYSTEM_CLOCK_FREQ 400000000UL

// 使能 UART 时钟
static void UART_EnableClock(UARTPort port) {
    uint32_t rcc_enr;
    uint32_t enable_bit;
    
    switch (port) {
        case UART_PORT_1:
        case UART_PORT_6:
            rcc_enr = RCC_BASE + RCC_APB2ENR_OFFSET;
            enable_bit = (port == UART_PORT_1) ? (1UL << 4) : (1UL << 5);
            break;
        case UART_PORT_2:
        case UART_PORT_3:
        case UART_PORT_4:
        case UART_PORT_5:
        case UART_PORT_7:
        case UART_PORT_8:
            rcc_enr = RCC_BASE + RCC_APB1LENR_OFFSET;
            switch (port) {
                case UART_PORT_2: enable_bit = 1UL << 17; break;
                case UART_PORT_3: enable_bit = 1UL << 18; break;
                case UART_PORT_4: enable_bit = 1UL << 19; break;
                case UART_PORT_5: enable_bit = 1UL << 20; break;
                case UART_PORT_7: enable_bit = 1UL << 30; break;
                case UART_PORT_8: enable_bit = 1UL << 31; break;
                default: return;
            }
            break;
        default:
            return;
    }
    
    REG32(rcc_enr) |= enable_bit;
    __asm__ __volatile__("dmb sy" ::: "memory");
}

// 计算波特率分频值
static uint32_t UART_CalculateBRR(uint32_t baud_rate, bool over8) {
    uint32_t clock_freq = SYSTEM_CLOCK_FREQ;
    uint32_t brr;
    
    if (over8) {
        brr = (2 * clock_freq + baud_rate / 2) / baud_rate;
        brr = (brr & 0xFFFFFFF0) | ((brr & 0x0F) >> 1);
    } else {
        brr = (clock_freq + baud_rate / 2) / baud_rate;
    }
    
    return brr;
}

void UART_Init(UARTPort port, const UARTConfig* config) {
    if (config == nullptr) {
        return;
    }
    
    UART_EnableClock(port);
    
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    
    // 先禁用 UART
    REG32(uart_base + USART_CR1_OFFSET) &= ~USART_CR1_UE;
    __asm__ __volatile__("dmb sy" ::: "memory");
    
    // 配置 CR1
    uint32_t cr1 = 0;
    
    // 数据位
    switch (config->data_bits) {
        case UART_DATA_BITS_8:
            cr1 &= ~(USART_CR1_M0 | USART_CR1_M1);
            break;
        case UART_DATA_BITS_9:
            cr1 |= USART_CR1_M0;
            cr1 &= ~USART_CR1_M1;
            break;
    }
    
    // 校验位
    switch (config->parity) {
        case UART_PARITY_NONE:
            cr1 &= ~USART_CR1_PCE;
            break;
        case UART_PARITY_EVEN:
            cr1 |= USART_CR1_PCE;
            cr1 &= ~USART_CR1_PS;
            break;
        case UART_PARITY_ODD:
            cr1 |= USART_CR1_PCE;
            cr1 |= USART_CR1_PS;
            break;
    }
    
    // 发送/接收使能
    if (config->tx_enable) {
        cr1 |= USART_CR1_TE;
    }
    if (config->rx_enable) {
        cr1 |= USART_CR1_RE;
    }
    
    REG32(uart_base + USART_CR1_OFFSET) = cr1;
    
    // 配置 CR2
    uint32_t cr2 = 0;
    
    // 停止位
    cr2 &= ~USART_CR2_STOP_MASK;
    cr2 |= static_cast<uint32_t>(config->stop_bits) << 12;
    
    REG32(uart_base + USART_CR2_OFFSET) = cr2;
    
    // 配置 CR3
    uint32_t cr3 = 0;
    
    // 流控制
    switch (config->flow_control) {
        case UART_FLOW_CONTROL_NONE:
            cr3 &= ~(USART_CR3_RTSE | USART_CR3_CTSE);
            break;
        case UART_FLOW_CONTROL_RTS:
            cr3 |= USART_CR3_RTSE;
            cr3 &= ~USART_CR3_CTSE;
            break;
        case UART_FLOW_CONTROL_CTS:
            cr3 &= ~USART_CR3_RTSE;
            cr3 |= USART_CR3_CTSE;
            break;
        case UART_FLOW_CONTROL_RTS_CTS:
            cr3 |= USART_CR3_RTSE | USART_CR3_CTSE;
            break;
    }
    
    REG32(uart_base + USART_CR3_OFFSET) = cr3;
    
    // 配置波特率
    bool over8 = (cr1 & USART_CR1_OVER8) != 0;
    uint32_t brr = UART_CalculateBRR(static_cast<uint32_t>(config->baud_rate), over8);
    REG32(uart_base + USART_BRR_OFFSET) = brr;
    
    // 启用 UART
    REG32(uart_base + USART_CR1_OFFSET) |= USART_CR1_UE;
    __asm__ __volatile__("dmb sy" ::: "memory");
}

void UART_DeInit(UARTPort port) {
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    
    // 禁用 UART
    REG32(uart_base + USART_CR1_OFFSET) &= ~USART_CR1_UE;
    __asm__ __volatile__("dmb sy" ::: "memory");
    
    // 重置所有寄存器
    REG32(uart_base + USART_CR1_OFFSET) = 0;
    REG32(uart_base + USART_CR2_OFFSET) = 0;
    REG32(uart_base + USART_CR3_OFFSET) = 0;
    REG32(uart_base + USART_BRR_OFFSET) = 0;
}

void UART_Enable(UARTPort port) {
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    REG32(uart_base + USART_CR1_OFFSET) |= USART_CR1_UE;
    __asm__ __volatile__("dmb sy" ::: "memory");
}

void UART_Disable(UARTPort port) {
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    REG32(uart_base + USART_CR1_OFFSET) &= ~USART_CR1_UE;
    __asm__ __volatile__("dmb sy" ::: "memory");
}

bool UART_WriteByte(UARTPort port, uint8_t byte) {
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    
    // 等待 TXE 标志
    uint32_t timeout = 100000;
    while (!(REG32(uart_base + USART_ISR_OFFSET) & USART_ISR_TXE)) {
        if (--timeout == 0) {
            return false;
        }
    }
    
    // 写入数据
    REG32(uart_base + USART_TDR_OFFSET) = static_cast<uint32_t>(byte);
    return true;
}

bool UART_ReadByte(UARTPort port, uint8_t* byte) {
    if (byte == nullptr) {
        return false;
    }
    
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    
    // 检查是否有数据
    if (!(REG32(uart_base + USART_ISR_OFFSET) & USART_ISR_RXNE)) {
        return false;
    }
    
    // 读取数据
    *byte = static_cast<uint8_t>(REG32(uart_base + USART_RDR_OFFSET));
    return true;
}

uint16_t UART_Write(UARTPort port, const uint8_t* data, uint16_t length) {
    if (data == nullptr || length == 0) {
        return 0;
    }
    
    uint16_t bytes_written = 0;
    for (uint16_t i = 0; i < length; i++) {
        if (UART_WriteByte(port, data[i])) {
            bytes_written++;
        } else {
            break;
        }
    }
    
    return bytes_written;
}

uint16_t UART_Read(UARTPort port, uint8_t* data, uint16_t length) {
    if (data == nullptr || length == 0) {
        return 0;
    }
    
    uint16_t bytes_read = 0;
    for (uint16_t i = 0; i < length; i++) {
        if (UART_ReadByte(port, &data[i])) {
            bytes_read++;
        } else {
            break;
        }
    }
    
    return bytes_read;
}

void UART_PrintString(UARTPort port, const char* str) {
    if (str == nullptr) {
        return;
    }
    
    while (*str != '\0') {
        UART_WriteByte(port, static_cast<uint8_t>(*str));
        str++;
    }
}

UARTStatus UART_GetStatus(UARTPort port) {
    UARTStatus status = {0};
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    uint32_t isr = REG32(uart_base + USART_ISR_OFFSET);
    
    status.tx_empty = (isr & USART_ISR_TXE) != 0;
    status.tx_complete = (isr & USART_ISR_TC) != 0;
    status.rx_not_empty = (isr & USART_ISR_RXNE) != 0;
    status.overrun_error = (isr & USART_ISR_ORE) != 0;
    status.parity_error = (isr & USART_ISR_PE) != 0;
    status.framing_error = (isr & USART_ISR_FE) != 0;
    status.break_detected = (isr & USART_ISR_LBDF) != 0;
    
    return status;
}

void UART_ClearErrors(UARTPort port) {
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    
    // 清除所有错误标志
    REG32(uart_base + USART_ICR_OFFSET) = 
        USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | 
        USART_ISR_ORE | USART_ISR_IDLE | USART_ISR_LBDF |
        USART_ISR_CTSIF | USART_ISR_RTOF | USART_ISR_EOBF |
        USART_ISR_CMF | USART_ISR_WUF;
}

void UART_EnableTxInterrupt(UARTPort port) {
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    REG32(uart_base + USART_CR1_OFFSET) |= USART_CR1_TXEIE;
}

void UART_DisableTxInterrupt(UARTPort port) {
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    REG32(uart_base + USART_CR1_OFFSET) &= ~USART_CR1_TXEIE;
}

void UART_EnableRxInterrupt(UARTPort port) {
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    REG32(uart_base + USART_CR1_OFFSET) |= USART_CR1_RXNEIE;
}

void UART_DisableRxInterrupt(UARTPort port) {
    uint32_t uart_base = UART_BASES[static_cast<uint32_t>(port)];
    REG32(uart_base + USART_CR1_OFFSET) &= ~USART_CR1_RXNEIE;
}
