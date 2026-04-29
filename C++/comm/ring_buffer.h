#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "../../include/shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// 循环队列大小 - 使用3个缓冲区，比原来的6个减少50%
// 状态：EMPTY → DMA_WRITE → PROCESSING → READY → EMPTY
#define RING_BUFFER_SIZE 3

// 缓冲区状态枚举
typedef enum {
    BUFFER_STATE_EMPTY = 0,
    BUFFER_STATE_DMA_WRITE = 1,
    BUFFER_STATE_READY = 2,
    BUFFER_STATE_PROCESSING = 3
} BufferState;

// 循环队列元素
typedef struct {
    // 图像数据 - 32字节对齐，适合DMA和Cache操作
    uint8_t data[IMAGE_SIZE] __attribute__((aligned(32)));
    uint32_t timestamp_ms;
    BufferState state;
    uint8_t frame_number;
    uint8_t reserved[3];
} RingBufferItem;

// 循环队列控制结构
typedef struct {
    RingBufferItem buffers[RING_BUFFER_SIZE];
    
    // 指针（索引）
    volatile uint8_t write_index;      // DMA写入的下一个位置
    volatile uint8_t read_index;       // 处理的下一个位置
    volatile uint8_t ready_count;      // 就绪的缓冲区数量
    
    // 统计信息
    uint32_t total_frames;
    uint32_t dropped_frames;
    uint32_t overflow_count;
    
    // 性能监控
    uint32_t min_processing_time;
    uint32_t max_processing_time;
    uint64_t total_processing_time;
    
    // 运行时间（用于1小时无衰减验证）
    uint64_t runtime_us;
    uint32_t runtime_checkpoints;
    
} RingBuffer;

// 全局循环队列实例（静态分配，无堆内存）
extern RingBuffer g_ring_buffer;

// 初始化函数
void RingBuffer_Init(RingBuffer* rb);
void RingBuffer_Reset(RingBuffer* rb);

// 获取用于DMA写入的缓冲区
// 返回缓冲区指针，如果没有可用缓冲区返回NULL
RingBufferItem* RingBuffer_GetWriteBuffer(RingBuffer* rb);

// 标记DMA写入完成
void RingBuffer_CommitWrite(RingBuffer* rb, uint32_t timestamp_ms);

// 获取用于处理的就绪缓冲区
// 返回缓冲区指针，如果没有就绪缓冲区返回NULL
RingBufferItem* RingBuffer_GetReadBuffer(RingBuffer* rb);

// 释放处理完成的缓冲区
void RingBuffer_ReleaseReadBuffer(RingBuffer* rb);

// 检查是否有就绪的缓冲区
bool RingBuffer_HasReady(const RingBuffer* rb);

// 获取就绪缓冲区数量
uint8_t RingBuffer_GetReadyCount(const RingBuffer* rb);

// 获取可用写入缓冲区数量
uint8_t RingBuffer_GetAvailableCount(const RingBuffer* rb);

// 统计函数
void RingBuffer_RecordProcessingTime(RingBuffer* rb, uint32_t time_ms);
uint32_t RingBuffer_GetAverageFPS(const RingBuffer* rb);
uint32_t RingBuffer_GetAverageProcessingTime(const RingBuffer* rb);
uint32_t RingBuffer_GetDroppedFrameRate(const RingBuffer* rb);

// 运行时间验证（确保1小时无性能衰减）
void RingBuffer_UpdateRuntime(RingBuffer* rb, uint32_t delta_us);
bool RingBuffer_CheckPerformanceDegradation(const RingBuffer* rb);
void RingBuffer_RecordCheckpoint(RingBuffer* rb);

// 内存占用信息
#define RING_BUFFER_MEMORY_SIZE (sizeof(RingBuffer))
// 原方案内存：4个DMA缓冲 + 2个SharedMemory缓冲 = 6 × 76800 + 管理开销
// 新方案内存：3个缓冲 = 3 × 76800 + 管理开销
// 内存减少：约50%，超过40%要求

#ifdef __cplusplus
}
#endif

#endif // RING_BUFFER_H
