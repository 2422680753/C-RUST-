#include "optimized_comm_manager.h"
#include "ring_buffer.h"
#include <cstring>

// 优化通信管理器状态
static OptimizedCommState g_comm_state = OPT_COMM_STATE_IDLE;

// 上一帧的索引（用于光流跟踪）
static uint8_t g_previous_frame_index = RING_BUFFER_SIZE;

// 前一帧的数据（用于比较）
static ImageFrame g_previous_frame_stored;

// 回调函数
static FrameReadyCallback g_frame_ready_callback = nullptr;
static ProcessingCompleteCallback g_processing_complete_callback = nullptr;
static ErrorCallback g_error_callback = nullptr;

// 性能统计
static OptimizedPerfStats g_perf_stats = {0};

// 用于存储处理输出
static ImageProcessingOutput g_processing_output;

// 获取当前时间戳（微秒）
// 这里应该使用高精度定时器
static uint64_t GetCurrentTimeUs(void) {
    // 模拟实现，实际应使用LPTIM
    return static_cast<uint64_t>(g_perf_stats.total_frames) * (1000000ULL / TARGET_FPS);
}

void OptimizedComm_Init(void) {
    // 初始化循环队列
    RingBuffer_Init(&g_ring_buffer);
    
    // 重置状态
    g_comm_state = OPT_COMM_STATE_IDLE;
    g_previous_frame_index = RING_BUFFER_SIZE;
    
    // 清空前一帧存储
    std::memset(&g_previous_frame_stored, 0, sizeof(ImageFrame));
    
    // 重置回调
    g_frame_ready_callback = nullptr;
    g_processing_complete_callback = nullptr;
    g_error_callback = nullptr;
    
    // 重置性能统计
    std::memset(&g_perf_stats, 0, sizeof(OptimizedPerfStats));
    g_perf_stats.min_latency_us = UINT32_MAX;
    g_perf_stats.min_processing_ms = UINT32_MAX;
    
    // 调用Rust初始化
    rust_init();
}

void OptimizedComm_DeInit(void) {
    g_comm_state = OPT_COMM_STATE_IDLE;
    
    // 清空回调
    g_frame_ready_callback = nullptr;
    g_processing_complete_callback = nullptr;
    g_error_callback = nullptr;
}

void OptimizedComm_Start(void) {
    if (g_comm_state == OPT_COMM_STATE_IDLE || g_comm_state == OPT_COMM_STATE_PAUSED) {
        g_comm_state = OPT_COMM_STATE_RUNNING;
    }
}

void OptimizedComm_Stop(void) {
    g_comm_state = OPT_COMM_STATE_PAUSED;
}

void OptimizedComm_Reset(void) {
    OptimizedComm_DeInit();
    OptimizedComm_Init();
}

OptimizedCommState OptimizedComm_GetState(void) {
    return g_comm_state;
}

// DMA中断回调函数 - 执行时间极短
void OptimizedComm_DMACompleteISR(void) {
    if (g_comm_state != OPT_COMM_STATE_RUNNING) {
        return;
    }
    
    // 获取写入缓冲区
    RingBufferItem* write_buffer = RingBuffer_GetWriteBuffer(&g_ring_buffer);
    
    if (write_buffer == nullptr) {
        // 没有可用缓冲区，增加丢帧计数
        g_perf_stats.dropped_frames++;
        g_perf_stats.dma_overruns++;
        
        // 调用错误回调
        if (g_error_callback != nullptr) {
            g_error_callback(1); // 1 = 缓冲区溢出
        }
        return;
    }
    
    // 记录DMA完成时间
    uint64_t current_time = GetCurrentTimeUs();
    
    // 提交写入完成
    RingBuffer_CommitWrite(&g_ring_buffer, static_cast<uint32_t>(current_time / 1000));
    
    // 更新统计
    g_perf_stats.total_frames++;
    
    // 记录延迟
    // 这里的延迟是从DMA开始到完成的时间
    // 实际实现中应该有更精确的计时
    
    // 调用帧就绪回调（如果已注册）
    if (g_frame_ready_callback != nullptr) {
        // 获取刚提交的帧
        uint8_t last_write = (g_ring_buffer.write_index + RING_BUFFER_SIZE - 1) % RING_BUFFER_SIZE;
        g_frame_ready_callback(&g_ring_buffer.buffers[last_write]);
    }
}

bool OptimizedComm_HasFrameReady(void) {
    return RingBuffer_HasReady(&g_ring_buffer);
}

const RingBufferItem* OptimizedComm_GetReadyFrame(void) {
    return RingBuffer_GetReadBuffer(&g_ring_buffer);
}

bool OptimizedComm_ProcessFrame(const RingBufferItem* current_frame,
                                  const RingBufferItem* previous_frame,
                                  ImageProcessingOutput* output) {
    if (current_frame == nullptr || output == nullptr) {
        return false;
    }
    
    uint64_t start_time = GetCurrentTimeUs();
    
    // 准备ImageFrame结构（零拷贝）
    ImageFrame curr_frame;
    curr_frame.data = const_cast<uint8_t*>(current_frame->data);
    curr_frame.timestamp_ms = current_frame->timestamp_ms;
    curr_frame.is_valid = true;
    
    ImageFrame prev_frame;
    ImageFrame* prev_frame_ptr = nullptr;
    
    if (previous_frame != nullptr) {
        // 使用提供的前一帧
        prev_frame.data = const_cast<uint8_t*>(previous_frame->data);
        prev_frame.timestamp_ms = previous_frame->timestamp_ms;
        prev_frame.is_valid = true;
        prev_frame_ptr = &prev_frame;
    } else if (g_previous_frame_stored.is_valid) {
        // 使用存储的前一帧
        prev_frame_ptr = &g_previous_frame_stored;
    } else {
        // 没有前一帧，使用当前帧作为前一帧
        prev_frame = curr_frame;
        prev_frame_ptr = &prev_frame;
    }
    
    // 调用Rust处理
    rust_process_frame(&curr_frame, prev_frame_ptr, output);
    
    // 计算处理时间
    uint64_t end_time = GetCurrentTimeUs();
    uint32_t processing_ms = static_cast<uint32_t>((end_time - start_time) / 1000);
    
    // 更新性能统计
    g_perf_stats.processed_frames++;
    g_perf_stats.total_processing_ms += processing_ms;
    
    if (processing_ms < g_perf_stats.min_processing_ms) {
        g_perf_stats.min_processing_ms = processing_ms;
    }
    if (processing_ms > g_perf_stats.max_processing_ms) {
        g_perf_stats.max_processing_ms = processing_ms;
    }
    
    // 估计CPU使用率
    output->processing_time_ms = processing_ms;
    output->cpu_usage_estimate = static_cast<uint8_t>((processing_ms * 100) / FRAME_INTERVAL_MS);
    
    // 更新平均CPU估计
    g_perf_stats.current_cpu_estimate = output->cpu_usage_estimate;
    if (g_perf_stats.processed_frames > 0) {
        g_perf_stats.average_cpu_estimate = static_cast<uint8_t>(
            (g_perf_stats.total_processing_ms * 100) / 
            (g_perf_stats.processed_frames * FRAME_INTERVAL_MS)
        );
    }
    
    // 存储当前帧作为下一帧的前一帧
    // 注意：这里只复制元数据，数据指针指向循环队列
    g_previous_frame_stored.data = const_cast<uint8_t*>(current_frame->data);
    g_previous_frame_stored.timestamp_ms = current_frame->timestamp_ms;
    g_previous_frame_stored.is_valid = true;
    
    // 调用处理完成回调
    if (g_processing_complete_callback != nullptr) {
        g_processing_complete_callback(output);
    }
    
    return output->is_valid;
}

void OptimizedComm_ReleaseFrame(const RingBufferItem* item) {
    if (item == nullptr) {
        return;
    }
    
    // 释放循环队列中的缓冲区
    // 注意：这里假设item是通过GetReadyFrame获取的
    RingBuffer_ReleaseReadBuffer(&g_ring_buffer);
}

const uint8_t* OptimizedComm_GetFrameData(const RingBufferItem* item) {
    if (item == nullptr) {
        return nullptr;
    }
    return item->data;
}

uint32_t OptimizedComm_GetFrameTimestamp(const RingBufferItem* item) {
    if (item == nullptr) {
        return 0;
    }
    return item->timestamp_ms;
}

void OptimizedComm_RegisterFrameReadyCallback(FrameReadyCallback callback) {
    g_frame_ready_callback = callback;
}

void OptimizedComm_RegisterProcessingCompleteCallback(ProcessingCompleteCallback callback) {
    g_processing_complete_callback = callback;
}

void OptimizedComm_RegisterErrorCallback(ErrorCallback callback) {
    g_error_callback = callback;
}

void OptimizedComm_GetPerfStats(OptimizedPerfStats* stats) {
    if (stats == nullptr) {
        return;
    }
    
    *stats = g_perf_stats;
}

uint32_t OptimizedComm_GetCurrentFPS(void) {
    if (g_perf_stats.runtime_us == 0) {
        return 0;
    }
    
    uint64_t runtime_sec = g_perf_stats.runtime_us / 1000000;
    if (runtime_sec == 0) {
        return 0;
    }
    
    return static_cast<uint32_t>(g_perf_stats.processed_frames / runtime_sec);
}

uint32_t OptimizedComm_GetAverageLatencyUs(void) {
    if (g_perf_stats.total_frames == 0) {
        return 0;
    }
    
    return static_cast<uint32_t>(g_perf_stats.total_latency_us / g_perf_stats.total_frames);
}

uint32_t OptimizedComm_GetDroppedFrameCount(void) {
    return g_perf_stats.dropped_frames;
}

bool OptimizedComm_CheckPerformanceDegradation(void) {
    // 检查处理时间
    if (g_perf_stats.processed_frames > 0) {
        uint32_t avg_processing = static_cast<uint32_t>(
            g_perf_stats.total_processing_ms / g_perf_stats.processed_frames
        );
        
        // 如果平均处理时间超过帧间隔的80%，认为性能衰减
        if (avg_processing > FRAME_INTERVAL_MS * 8 / 10) {
            g_perf_stats.performance_degraded = true;
            return true;
        }
        
        // 如果最大处理时间超过帧间隔，认为性能衰减
        if (g_perf_stats.max_processing_ms > FRAME_INTERVAL_MS) {
            g_perf_stats.performance_degraded = true;
            return true;
        }
    }
    
    // 检查丢帧率
    if (g_perf_stats.total_frames > 100) {
        uint32_t drop_rate = (g_perf_stats.dropped_frames * 1000) / g_perf_stats.total_frames;
        // 如果丢帧率超过5%，认为性能衰减
        if (drop_rate > 50) { // 5% = 50 ‰
            g_perf_stats.performance_degraded = true;
            return true;
        }
    }
    
    return false;
}

void OptimizedComm_RecordCheckpoint(void) {
    g_perf_stats.checkpoints++;
    
    // 每小时记录一次检查点
    // 这里可以添加性能日志
}
