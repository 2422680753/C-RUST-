#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// DMA 流定义
typedef enum {
    DMA_STREAM_0 = 0,
    DMA_STREAM_1 = 1,
    DMA_STREAM_2 = 2,
    DMA_STREAM_3 = 3,
    DMA_STREAM_4 = 4,
    DMA_STREAM_5 = 5,
    DMA_STREAM_6 = 6,
    DMA_STREAM_7 = 7
} DMAStream;

// DMA 控制器定义
typedef enum {
    DMA_CONTROLLER_1 = 0,
    DMA_CONTROLLER_2 = 1
} DMAController;

// DMA 数据传输方向
typedef enum {
    DMA_DIR_PERIPH_TO_MEM = 0,
    DMA_DIR_MEM_TO_PERIPH = 1,
    DMA_DIR_MEM_TO_MEM = 2
} DMADirection;

// DMA 数据宽度
typedef enum {
    DMA_DATA_WIDTH_8_BIT = 0,
    DMA_DATA_WIDTH_16_BIT = 1,
    DMA_DATA_WIDTH_32_BIT = 2
} DMADataWidth;

// DMA 优先级
typedef enum {
    DMA_PRIORITY_LOW = 0,
    DMA_PRIORITY_MEDIUM = 1,
    DMA_PRIORITY_HIGH = 2,
    DMA_PRIORITY_VERY_HIGH = 3
} DMAPriority;

// DMA 突发传输配置
typedef enum {
    DMA_BURST_SINGLE = 0,
    DMA_BURST_INCR4 = 1,
    DMA_BURST_INCR8 = 2,
    DMA_BURST_INCR16 = 3
} DMABurst;

// DMA FIFO 阈值
typedef enum {
    DMA_FIFO_THRESHOLD_1_4 = 0,
    DMA_FIFO_THRESHOLD_1_2 = 1,
    DMA_FIFO_THRESHOLD_3_4 = 2,
    DMA_FIFO_THRESHOLD_FULL = 3
} DMAFIFOThreshold;

// DMA 双缓冲状态
typedef enum {
    DMA_BUFFER_0 = 0,
    DMA_BUFFER_1 = 1
} DMABufferIndex;

// DMA 配置结构
typedef struct {
    DMADirection direction;
    DMADataWidth periph_data_width;
    DMADataWidth mem_data_width;
    DMAPriority priority;
    DMABurst periph_burst;
    DMABurst mem_burst;
    DMAFIFOThreshold fifo_threshold;
    bool fifo_enable;
    bool periph_inc;
    bool mem_inc;
    bool circular_mode;
    bool double_buffer_mode;
    bool transfer_complete_interrupt;
    bool half_transfer_interrupt;
    bool transfer_error_interrupt;
    bool direct_mode_error_interrupt;
    uint8_t channel;
} DMAConfig;

// DMA 传输状态
typedef struct {
    bool transfer_complete;
    bool half_transfer;
    bool transfer_error;
    bool direct_mode_error;
    bool fifo_error;
    DMABufferIndex current_buffer;
    uint32_t remaining_transfers;
} DMAStatus;

// 专用 DMA 流定义
#define DMA_CAMERA_STREAM DMA_STREAM_0
#define DMA_CAMERA_CONTROLLER DMA_CONTROLLER_2
#define DMA_CAMERA_CHANNEL 1

// DMA 函数声明
void DMA_Init(DMAController controller, DMAStream stream, const DMAConfig* config);
void DMA_DeInit(DMAController controller, DMAStream stream);

void DMA_Start(DMAController controller, DMAStream stream);
void DMA_Stop(DMAController controller, DMAStream stream);

void DMA_SetPeripheralAddress(DMAController controller, DMAStream stream, uint32_t address);
void DMA_SetMemory0Address(DMAController controller, DMAStream stream, uint32_t address);
void DMA_SetMemory1Address(DMAController controller, DMAStream stream, uint32_t address);
void DMA_SetDataLength(DMAController controller, DMAStream stream, uint16_t length);

void DMA_EnableInterrupt(DMAController controller, DMAStream stream);
void DMA_DisableInterrupt(DMAController controller, DMAStream stream);

DMAStatus DMA_GetStatus(DMAController controller, DMAStream stream);
void DMA_ClearInterruptFlags(DMAController controller, DMAStream stream);

DMABufferIndex DMA_GetCurrentBuffer(DMAController controller, DMAStream stream);
void DMA_SwitchMemoryTarget(DMAController controller, DMAStream stream);

// 摄像头 DMA 双缓冲专用函数
void DMA_CameraDoubleBuffer_Init(uint32_t peripheral_addr, 
                                  uint32_t buffer0_addr, 
                                  uint32_t buffer1_addr,
                                  uint16_t buffer_size);
void DMA_CameraDoubleBuffer_Start(void);
void DMA_CameraDoubleBuffer_Stop(void);
DMABufferIndex DMA_CameraDoubleBuffer_GetReadyBuffer(void);
bool DMA_CameraDoubleBuffer_HasNewFrame(void);
void DMA_CameraDoubleBuffer_ClearNewFrameFlag(void);

// 中断回调类型
typedef void (*DMA_TransferCompleteCallback)(DMAController controller, DMAStream stream);
typedef void (*DMA_TransferErrorCallback)(DMAController controller, DMAStream stream, uint32_t error_code);

void DMA_RegisterTransferCompleteCallback(DMAController controller, DMAStream stream, 
                                           DMA_TransferCompleteCallback callback);
void DMA_RegisterTransferErrorCallback(DMAController controller, DMAStream stream,
                                        DMA_TransferErrorCallback callback);

#ifdef __cplusplus
}
#endif

#endif // DMA_H
