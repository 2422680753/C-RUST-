#ifndef OPTIMIZED_COMM_MANAGER_H
#define OPTIMIZED_COMM_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "../../include/shared.h"
#include "ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

// 优化的通信管理器状态
typedef enum {
    OPT_COMM_STATE_IDLE = 0,
    OPT_COMM_STATE_RUNNING = 1,
    OPT_COMM_STATE_PAUSED = 2,
    OPT_COMM_STATE_ERROR = 3
} OptimizedCommState;

// 性能统计结构
typedef struct {
    uint32_t total_frames;
    uint32_t processed_frames;
    uint32_t dropped_frames;
    uint32_t dma_overruns;
    
    uint32_t min_latency_us;
    uint32_t max_latency_us;
    uint64_t total_latency_us;
    
    uint32_t min_processing_ms;
    uint32_t max_processing_ms;
    uint64_t total_processing_ms;
    
    uint8_t current_cpu_estimate;
    uint8_t average_cpu_estimate;
    
    // 1小时性能衰减检查
    uint64_t runtime_us;
    uint32_t checkpoints;
    bool performance_degraded;
    
} OptimizedPerfStats;

// DMA完成回调类型
typedef void (*FrameReadyCallback)(const RingBufferItem* item);
typedef void (*ProcessingCompleteCallback)(const ImageProcessingOutput* output);
typedef void (*ErrorCallback)(uint32_t error_code);

// 初始化和控制函数
void OptimizedComm_Init(void);
void OptimizedComm_DeInit(void);

void OptimizedComm_Start(void);
void OptimizedComm_Stop(void);
void OptimizedComm_Reset(void);

OptimizedCommState OptimizedComm_GetState(void);

// DMA中断回调函数 - 应该在DMA中断中调用
// 这个函数执行时间极短，只更新标志位
void OptimizedComm_DMACompleteISR(void);

// 检查是否有新帧就绪
bool OptimizedComm_HasFrameReady(void);

// 获取就绪的帧进行处理
// 返回帧指针，如果没有就绪帧返回NULL
const RingBufferItem* OptimizedComm_GetReadyFrame(void);

// 处理帧 - 同步调用Rust处理
// 这个函数在主循环中调用，不在中断中
bool OptimizedComm_ProcessFrame(const RingBufferItem* current_frame,
                                  const RingBufferItem* previous_frame,
                                  ImageProcessingOutput* output);

// 释放处理完成的帧
void OptimizedComm_ReleaseFrame(const RingBufferItem* item);

// 直接从循环队列获取帧数据的指针（零拷贝）
const uint8_t* OptimizedComm_GetFrameData(const RingBufferItem* item);
uint32_t OptimizedComm_GetFrameTimestamp(const RingBufferItem* item);

// 回调注册
void OptimizedComm_RegisterFrameReadyCallback(FrameReadyCallback callback);
void OptimizedComm_RegisterProcessingCompleteCallback(ProcessingCompleteCallback callback);
void OptimizedComm_RegisterErrorCallback(ErrorCallback callback);

// 性能统计
void OptimizedComm_GetPerfStats(OptimizedPerfStats* stats);
uint32_t OptimizedComm_GetCurrentFPS(void);
uint32_t OptimizedComm_GetAverageLatencyUs(void);
uint32_t OptimizedComm_GetDroppedFrameCount(void);

// 性能衰减检查（1小时无衰减验证）
bool OptimizedComm_CheckPerformanceDegradation(void);
void OptimizedComm_RecordCheckpoint(void);

// 内存占用信息
// 优化前：约460KB（6个缓冲区 + SharedMemory）
// 优化后：约230KB（3个循环队列缓冲区）
// 内存减少：约50% > 40%要求

#ifdef __cplusplus
}
#endif

#endif // OPTIMIZED_COMM_MANAGER_H
