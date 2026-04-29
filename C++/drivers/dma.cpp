#include "dma.h"
#include <cstring>

// STM32H7 DMA 寄存器基地址
#define DMA1_BASE 0x40020000
#define DMA2_BASE 0x40020400

// DMA 寄存器偏移
#define DMA_LISR_OFFSET 0x00
#define DMA_HISR_OFFSET 0x04
#define DMA_LIFCR_OFFSET 0x08
#define DMA_HIFCR_OFFSET 0x0C

// DMA 流寄存器偏移
#define DMA_STREAM_CR_OFFSET 0x10
#define DMA_STREAM_NDTR_OFFSET 0x14
#define DMA_STREAM_PAR_OFFSET 0x18
#define DMA_STREAM_M0AR_OFFSET 0x1C
#define DMA_STREAM_M1AR_OFFSET 0x20
#define DMA_STREAM_FCR_OFFSET 0x24

// DMA 流间隔（每个流 0x18 字节）
#define DMA_STREAM_OFFSET(stream) (0x10 + ((stream) * 0x18)

// DMA SxCR 寄存器位定义
#define DMA_SxCR_EN (1UL << 0)
#define DMA_SxCR_DMEIE (1UL << 1)
#define DMA_SxCR_TEIE (1UL << 2)
#define DMA_SxCR_HTIE (1UL << 3)
#define DMA_SxCR_TCIE (1UL << 4)
#define DMA_SxCR_PFCTRL (1UL << 5)
#define DMA_SxCR_DIR_MASK (3UL << 6)
#define DMA_SxCR_CIRC (1UL << 8)
#define DMA_SxCR_PINC (1UL << 9)
#define DMA_SxCR_MINC (1UL << 10)
#define DMA_SxCR_PSIZE_MASK (3UL << 11)
#define DMA_SxCR_MSIZE_MASK (3UL << 13)
#define DMA_SxCR_PINCOS (1UL << 15)
#define DMA_SxCR_PL_MASK (3UL << 16)
#define DMA_SxCR_DBM (1UL << 18)
#define DMA_SxCR_CT (1UL << 19)
#define DMA_SxCR_PBURST_MASK (3UL << 21)
#define DMA_SxCR_MBURST_MASK (3UL << 23)
#define DMA_SxCR_CHSEL_MASK (7UL << 25)
#define DMA_SxCR_TRBUFF (1UL << 20)

// DMA SxFCR 寄存器位定义
#define DMA_SxFCR_FTH_MASK (3UL << 0)
#define DMA_SxFCR_DMDIS (1UL << 2)
#define DMA_SxFCR_FS_MASK (7UL << 3)
#define DMA_SxFCR_FEIE (1UL << 7)

// DMA 状态寄存器位定义
#define DMA_LISR_FEIF0 (1UL << 0)
#define DMA_LISR_DMEIF0 (1UL << 2)
#define DMA_LISR_TEIF0 (1UL << 3)
#define DMA_LISR_HTIF0 (1UL << 4)
#define DMA_LISR_TCIF0 (1UL << 5)

// 每流的位偏移
#define DMA_STREAM_BIT_OFFSET(stream) (((stream) % 4 ? (((stream) % 4 * 6)

// 寄存器访问宏
#define REG32(addr) (*(volatile uint32_t*)(addr))

// DMA 控制器基地址表
static const uint32_t DMA_BASES[] = {
    DMA1_BASE,
    DMA2_BASE
};

// 回调函数表
static DMA_TransferCompleteCallback g_tc_callbacks[2][8] = {{nullptr}};
static DMA_TransferErrorCallback g_te_callbacks[2][8] = {{nullptr}};

// 摄像头双缓冲状态
static volatile bool g_camera_new_frame = false;
static volatile DMABufferIndex g_camera_ready_buffer = DMA_BUFFER_0;
static volatile DMABufferIndex g_camera_current_buffer = DMA_BUFFER_0;

// 计算流的状态寄存器位
static uint32_t DMA_GetStreamStatusBit(DMAController controller, DMAStream stream) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    
    uint32_t bit_offset;
    uint32_t isr_offset;
    
    if (stream_num < 4) {
        bit_offset = stream_num * 6;
        isr_offset = DMA_LISR_OFFSET;
    } else {
        bit_offset = (stream_num - 4) * 6;
        isr_offset = DMA_HISR_OFFSET;
    }
    
    return REG32(dma_base + isr_offset);
}

static void DMA_ClearStreamInterruptFlags(DMAController controller, DMAStream stream) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    
    uint32_t bit_offset;
    uint32_t ifcr_offset;
    
    if (stream_num < 4) {
        bit_offset = stream_num * 6;
        ifcr_offset = DMA_LIFCR_OFFSET;
    } else {
        bit_offset = (stream_num - 4) * 6;
        ifcr_offset = DMA_HIFCR_OFFSET;
    }
    
    // 清除所有中断标志
    uint32_t clear_mask = (0x3FUL << bit_offset);
    REG32(dma_base + ifcr_offset) = clear_mask;
}

void DMA_Init(DMAController controller, DMAStream stream, const DMAConfig* config) {
    if (config == nullptr) {
        return;
    }
    
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    // 禁用流
    REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) &= ~DMA_SxCR_EN;
    while (REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) & DMA_SxCR_EN) {
        // 等待禁用
    }
    
    // 清除中断标志
    DMA_ClearStreamInterruptFlags(controller, stream);
    
    // 配置 SxCR
    uint32_t cr = 0;
    
    // 方向
    cr &= ~DMA_SxCR_DIR_MASK;
    cr |= static_cast<uint32_t>(config->direction) << 6;
    
    // 外设数据宽度
    cr &= ~DMA_SxCR_PSIZE_MASK;
    cr |= static_cast<uint32_t>(config->periph_data_width) << 11;
    
    // 存储器数据宽度
    cr &= ~DMA_SxCR_MSIZE_MASK;
    cr |= static_cast<uint32_t>(config->mem_data_width) << 13;
    
    // 优先级
    cr &= ~DMA_SxCR_PL_MASK;
    cr |= static_cast<uint32_t>(config->priority) << 16;
    
    // 外设突发
    cr &= ~DMA_SxCR_PBURST_MASK;
    cr |= static_cast<uint32_t>(config->periph_burst) << 21;
    
    // 存储器突发
    cr &= ~DMA_SxCR_MBURST_MASK;
    cr |= static_cast<uint32_t>(config->mem_burst) << 23;
    
    // 通道选择
    cr &= ~DMA_SxCR_CHSEL_MASK;
    cr |= static_cast<uint32_t>(config->channel) << 25;
    
    // 外设增量
    if (config->periph_inc) {
        cr |= DMA_SxCR_PINC;
    } else {
        cr &= ~DMA_SxCR_PINC;
    }
    
    // 存储器增量
    if (config->mem_inc) {
        cr |= DMA_SxCR_MINC;
    } else {
        cr &= ~DMA_SxCR_MINC;
    }
    
    // 循环模式
    if (config->circular_mode) {
        cr |= DMA_SxCR_CIRC;
    } else {
        cr &= ~DMA_SxCR_CIRC;
    }
    
    // 双缓冲模式
    if (config->double_buffer_mode) {
        cr |= DMA_SxCR_DBM;
    } else {
        cr &= ~DMA_SxCR_DBM;
    }
    
    // 中断使能
    if (config->transfer_complete_interrupt) {
        cr |= DMA_SxCR_TCIE;
    } else {
        cr &= ~DMA_SxCR_TCIE;
    }
    
    if (config->half_transfer_interrupt) {
        cr |= DMA_SxCR_HTIE;
    } else {
        cr &= ~DMA_SxCR_HTIE;
    }
    
    if (config->transfer_error_interrupt) {
        cr |= DMA_SxCR_TEIE;
    } else {
        cr &= ~DMA_SxCR_TEIE;
    }
    
    if (config->direct_mode_error_interrupt) {
        cr |= DMA_SxCR_DMEIE;
    } else {
        cr &= ~DMA_SxCR_DMEIE;
    }
    
    REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) = cr;
    
    // 配置 FCR
    uint32_t fcr = 0;
    
    // FIFO 阈值
    fcr &= ~DMA_SxFCR_FTH_MASK;
    fcr |= static_cast<uint32_t>(config->fifo_threshold);
    
    // FIFO 使能
    if (config->fifo_enable) {
        fcr |= DMA_SxFCR_DMDIS;
    } else {
        fcr &= ~DMA_SxFCR_DMDIS;
    }
    
    REG32(dma_base + stream_offset + DMA_STREAM_FCR_OFFSET) = fcr;
}

void DMA_DeInit(DMAController controller, DMAStream stream) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    // 禁用流
    REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) &= ~DMA_SxCR_EN;
    
    // 等待禁用
    while (REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) & DMA_SxCR_EN) {
    }
    
    // 重置寄存器
    REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) = 0;
    REG32(dma_base + stream_offset + DMA_STREAM_NDTR_OFFSET) = 0;
    REG32(dma_base + stream_offset + DMA_STREAM_PAR_OFFSET) = 0;
    REG32(dma_base + stream_offset + DMA_STREAM_M0AR_OFFSET) = 0;
    REG32(dma_base + stream_offset + DMA_STREAM_M1AR_OFFSET) = 0;
    REG32(dma_base + stream_offset + DMA_STREAM_FCR_OFFSET) = 0x21;
    
    // 清除中断标志
    DMA_ClearStreamInterruptFlags(controller, stream);
    
    // 清除回调
    g_tc_callbacks[static_cast<uint32_t>(controller)][static_cast<uint32_t>(stream)] = nullptr;
    g_te_callbacks[static_cast<uint32_t>(controller)][static_cast<uint32_t>(stream)] = nullptr;
}

void DMA_Start(DMAController controller, DMAStream stream) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) |= DMA_SxCR_EN;
    __asm__ __volatile__("dmb sy" ::: "memory");
}

void DMA_Stop(DMAController controller, DMAStream stream) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) &= ~DMA_SxCR_EN;
    
    // 等待禁用
    while (REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) & DMA_SxCR_EN) {
    }
}

void DMA_SetPeripheralAddress(DMAController controller, DMAStream stream, uint32_t address) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    REG32(dma_base + stream_offset + DMA_STREAM_PAR_OFFSET) = address;
}

void DMA_SetMemory0Address(DMAController controller, DMAStream stream, uint32_t address) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    REG32(dma_base + stream_offset + DMA_STREAM_M0AR_OFFSET) = address;
}

void DMA_SetMemory1Address(DMAController controller, DMAStream stream, uint32_t address) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    REG32(dma_base + stream_offset + DMA_STREAM_M1AR_OFFSET) = address;
}

void DMA_SetDataLength(DMAController controller, DMAStream stream, uint16_t length) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    REG32(dma_base + stream_offset + DMA_STREAM_NDTR_OFFSET) = static_cast<uint32_t>(length);
}

void DMA_EnableInterrupt(DMAController controller, DMAStream stream) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    uint32_t cr = REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET);
    cr |= DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) = cr;
}

void DMA_DisableInterrupt(DMAController controller, DMAStream stream) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    uint32_t cr = REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET);
    cr &= ~(DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE | DMA_SxCR_HTIE);
    REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) = cr;
}

DMAStatus DMA_GetStatus(DMAController controller, DMAStream stream) {
    DMAStatus status = {0};
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    uint32_t isr = DMA_GetStreamStatusBit(controller, stream);
    uint32_t bit_offset = (stream_num < 4) ? (stream_num * 6) : ((stream_num - 4) * 6);
    
    status.transfer_complete = (isr & (DMA_LISR_TCIF0 << bit_offset)) != 0;
    status.half_transfer = (isr & (DMA_LISR_HTIF0 << bit_offset)) != 0;
    status.transfer_error = (isr & (DMA_LISR_TEIF0 << bit_offset)) != 0;
    status.direct_mode_error = (isr & (DMA_LISR_DMEIF0 << bit_offset)) != 0;
    status.fifo_error = (isr & (DMA_LISR_FEIF0 << bit_offset)) != 0;
    
    // 当前缓冲区（双缓冲模式）
    uint32_t cr = REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET);
    status.current_buffer = (cr & DMA_SxCR_CT) ? DMA_BUFFER_1 : DMA_BUFFER_0;
    
    // 剩余传输数
    status.remaining_transfers = REG32(dma_base + stream_offset + DMA_STREAM_NDTR_OFFSET);
    
    return status;
}

void DMA_ClearInterruptFlags(DMAController controller, DMAStream stream) {
    DMA_ClearStreamInterruptFlags(controller, stream);
}

DMABufferIndex DMA_GetCurrentBuffer(DMAController controller, DMAStream stream) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    uint32_t cr = REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET);
    return (cr & DMA_SxCR_CT) ? DMA_BUFFER_1 : DMA_BUFFER_0;
}

void DMA_SwitchMemoryTarget(DMAController controller, DMAStream stream) {
    uint32_t dma_base = DMA_BASES[static_cast<uint32_t>(controller)];
    uint32_t stream_num = static_cast<uint32_t>(stream);
    uint32_t stream_offset = DMA_STREAM_OFFSET(stream_num);
    
    uint32_t cr = REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET);
    cr ^= DMA_SxCR_CT;
    REG32(dma_base + stream_offset + DMA_STREAM_CR_OFFSET) = cr;
}

// 摄像头双缓冲专用实现
void DMA_CameraDoubleBuffer_Init(uint32_t peripheral_addr, 
                                  uint32_t buffer0_addr, 
                                  uint32_t buffer1_addr,
                                  uint16_t buffer_size) {
    DMAConfig dma_config = {
        .direction = DMA_DIR_PERIPH_TO_MEM,
        .periph_data_width = DMA_DATA_WIDTH_8_BIT,
        .mem_data_width = DMA_DATA_WIDTH_8_BIT,
        .priority = DMA_PRIORITY_HIGH,
        .periph_burst = DMA_BURST_SINGLE,
        .mem_burst = DMA_BURST_SINGLE,
        .fifo_threshold = DMA_FIFO_THRESHOLD_FULL,
        .fifo_enable = true,
        .periph_inc = false,
        .mem_inc = true,
        .circular_mode = true,
        .double_buffer_mode = true,
        .transfer_complete_interrupt = true,
        .half_transfer_interrupt = false,
        .transfer_error_interrupt = true,
        .direct_mode_error_interrupt = true,
        .channel = DMA_CAMERA_CHANNEL
    };
    
    DMA_Init(DMA_CAMERA_CONTROLLER, DMA_CAMERA_STREAM, &dma_config);
    
    DMA_SetPeripheralAddress(DMA_CAMERA_CONTROLLER, DMA_CAMERA_STREAM, peripheral_addr);
    DMA_SetMemory0Address(DMA_CAMERA_CONTROLLER, DMA_CAMERA_STREAM, buffer0_addr);
    DMA_SetMemory1Address(DMA_CAMERA_CONTROLLER, DMA_CAMERA_STREAM, buffer1_addr);
    DMA_SetDataLength(DMA_CAMERA_CONTROLLER, DMA_CAMERA_STREAM, buffer_size);
    
    g_camera_new_frame = false;
    g_camera_ready_buffer = DMA_BUFFER_0;
    g_camera_current_buffer = DMA_BUFFER_0;
}

void DMA_CameraDoubleBuffer_Start(void) {
    g_camera_new_frame = false;
    g_camera_ready_buffer = DMA_BUFFER_0;
    g_camera_current_buffer = DMA_BUFFER_0;
    DMA_Start(DMA_CAMERA_CONTROLLER, DMA_CAMERA_STREAM);
}

void DMA_CameraDoubleBuffer_Stop(void) {
    DMA_Stop(DMA_CAMERA_CONTROLLER, DMA_CAMERA_STREAM);
}

DMABufferIndex DMA_CameraDoubleBuffer_GetReadyBuffer(void) {
    return g_camera_ready_buffer;
}

bool DMA_CameraDoubleBuffer_HasNewFrame(void) {
    return g_camera_new_frame;
}

void DMA_CameraDoubleBuffer_ClearNewFrameFlag(void) {
    g_camera_new_frame = false;
}

// 回调注册
void DMA_RegisterTransferCompleteCallback(DMAController controller, DMAStream stream, 
                                           DMA_TransferCompleteCallback callback) {
    g_tc_callbacks[static_cast<uint32_t>(controller)][static_cast<uint32_t>(stream)] = callback;
}

void DMA_RegisterTransferErrorCallback(DMAController controller, DMAStream stream,
                                        DMA_TransferErrorCallback callback) {
    g_te_callbacks[static_cast<uint32_t>(controller)][static_cast<uint32_t>(stream)] = callback;
}

// DMA 中断处理函数（需要在中断向量表中引用）
extern "C" {
    
static void DMA_HandleIRQ(DMAController controller, DMAStream stream) {
    DMAStatus status = DMA_GetStatus(controller, stream);
    
    if (status.transfer_complete) {
        // 清除标志
        DMA_ClearInterruptFlags(controller, stream);
        
        // 摄像头双缓冲特殊处理
        if (controller == DMA_CAMERA_CONTROLLER && stream == DMA_CAMERA_STREAM) {
            // 切换缓冲区
            g_camera_current_buffer = (g_camera_current_buffer == DMA_BUFFER_0) ? 
                                        DMA_BUFFER_1 : DMA_BUFFER_0;
            g_camera_ready_buffer = g_camera_current_buffer;
            g_camera_new_frame = true;
        }
        
        // 调用回调
        DMA_TransferCompleteCallback callback = 
            g_tc_callbacks[static_cast<uint32_t>(controller)][static_cast<uint32_t>(stream)];
        if (callback != nullptr) {
            callback(controller, stream);
        }
    }
    
    if (status.transfer_error || status.direct_mode_error || status.fifo_error) {
        DMA_ClearInterruptFlags(controller, stream);
        
        DMA_TransferErrorCallback callback = 
            g_te_callbacks[static_cast<uint32_t>(controller)][static_cast<uint32_t>(stream)];
        if (callback != nullptr) {
            uint32_t error_code = 0;
            if (status.transfer_error) error_code |= 0x01;
            if (status.direct_mode_error) error_code |= 0x02;
            if (status.fifo_error) error_code |= 0x04;
            callback(controller, stream, error_code);
        }
    }
}

// DMA 中断处理函数
void DMA1_Stream0_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_1, DMA_STREAM_0); }
void DMA1_Stream1_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_1, DMA_STREAM_1); }
void DMA1_Stream2_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_1, DMA_STREAM_2); }
void DMA1_Stream3_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_1, DMA_STREAM_3); }
void DMA1_Stream4_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_1, DMA_STREAM_4); }
void DMA1_Stream5_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_1, DMA_STREAM_5); }
void DMA1_Stream6_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_1, DMA_STREAM_6); }
void DMA1_Stream7_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_1, DMA_STREAM_7); }

void DMA2_Stream0_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_2, DMA_STREAM_0); }
void DMA2_Stream1_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_2, DMA_STREAM_1); }
void DMA2_Stream2_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_2, DMA_STREAM_2); }
void DMA2_Stream3_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_2, DMA_STREAM_3); }
void DMA2_Stream4_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_2, DMA_STREAM_4); }
void DMA2_Stream5_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_2, DMA_STREAM_5); }
void DMA2_Stream6_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_2, DMA_STREAM_6); }
void DMA2_Stream7_IRQHandler(void) { DMA_HandleIRQ(DMA_CONTROLLER_2, DMA_STREAM_7); }

}
