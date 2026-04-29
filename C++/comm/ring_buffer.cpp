#include "ring_buffer.h"
#include <cstring>

// 全局循环队列实例
RingBuffer g_ring_buffer;

void RingBuffer_Init(RingBuffer* rb) {
    if (rb == nullptr) {
        return;
    }
    
    // 清空所有缓冲区
    std::memset(rb->buffers, 0, sizeof(rb->buffers));
    
    // 初始化指针
    rb->write_index = 0;
    rb->read_index = 0;
    rb->ready_count = 0;
    
    // 初始化所有缓冲区状态
    for (uint8_t i = 0; i < RING_BUFFER_SIZE; i++) {
        rb->buffers[i].state = BUFFER_STATE_EMPTY;
        rb->buffers[i].frame_number = 0;
    }
    
    // 重置统计
    rb->total_frames = 0;
    rb->dropped_frames = 0;
    rb->overflow_count = 0;
    
    // 重置性能监控
    rb->min_processing_time = UINT32_MAX;
    rb->max_processing_time = 0;
    rb->total_processing_time = 0;
    
    // 重置运行时间
    rb->runtime_us = 0;
    rb->runtime_checkpoints = 0;
}

void RingBuffer_Reset(RingBuffer* rb) {
    RingBuffer_Init(rb);
}

RingBufferItem* RingBuffer_GetWriteBuffer(RingBuffer* rb) {
    if (rb == nullptr) {
        return nullptr;
    }
    
    uint8_t current_write = rb->write_index;
    
    // 检查当前写指针指向的缓冲区状态
    if (rb->buffers[current_write].state == BUFFER_STATE_EMPTY) {
        // 标记为DMA写入中
        rb->buffers[current_write].state = BUFFER_STATE_DMA_WRITE;
        return &rb->buffers[current_write];
    }
    
    // 检查是否所有缓冲区都被占用
    if (rb->ready_count >= RING_BUFFER_SIZE - 1) {
        // 溢出，丢弃最旧的就绪帧
        uint8_t oldest_ready = rb->read_index;
        if (rb->buffers[oldest_ready].state == BUFFER_STATE_READY) {
            rb->buffers[oldest_ready].state = BUFFER_STATE_EMPTY;
            rb->read_index = (rb->read_index + 1) % RING_BUFFER_SIZE;
            rb->ready_count--;
            rb->dropped_frames++;
            rb->overflow_count++;
        }
    }
    
    // 再次尝试获取
    current_write = rb->write_index;
    if (rb->buffers[current_write].state == BUFFER_STATE_EMPTY) {
        rb->buffers[current_write].state = BUFFER_STATE_DMA_WRITE;
        return &rb->buffers[current_write];
    }
    
    return nullptr;
}

void RingBuffer_CommitWrite(RingBuffer* rb, uint32_t timestamp_ms) {
    if (rb == nullptr) {
        return;
    }
    
    uint8_t current_write = rb->write_index;
    
    // 验证状态
    if (rb->buffers[current_write].state != BUFFER_STATE_DMA_WRITE) {
        return;
    }
    
    // 设置时间戳
    rb->buffers[current_write].timestamp_ms = timestamp_ms;
    rb->buffers[current_write].frame_number = static_cast<uint8_t>(rb->total_frames & 0xFF);
    
    // 标记为就绪
    rb->buffers[current_write].state = BUFFER_STATE_READY;
    rb->ready_count++;
    rb->total_frames++;
    
    // 移动写指针
    rb->write_index = (rb->write_index + 1) % RING_BUFFER_SIZE;
}

RingBufferItem* RingBuffer_GetReadBuffer(RingBuffer* rb) {
    if (rb == nullptr) {
        return nullptr;
    }
    
    if (rb->ready_count == 0) {
        return nullptr;
    }
    
    uint8_t current_read = rb->read_index;
    
    // 检查是否有就绪的缓冲区
    if (rb->buffers[current_read].state == BUFFER_STATE_READY) {
        // 标记为处理中
        rb->buffers[current_read].state = BUFFER_STATE_PROCESSING;
        return &rb->buffers[current_read];
    }
    
    return nullptr;
}

void RingBuffer_ReleaseReadBuffer(RingBuffer* rb) {
    if (rb == nullptr) {
        return;
    }
    
    uint8_t current_read = rb->read_index;
    
    // 验证状态
    if (rb->buffers[current_read].state != BUFFER_STATE_PROCESSING) {
        return;
    }
    
    // 标记为空
    rb->buffers[current_read].state = BUFFER_STATE_EMPTY;
    
    // 移动读指针
    rb->read_index = (rb->read_index + 1) % RING_BUFFER_SIZE;
    rb->ready_count--;
}

bool RingBuffer_HasReady(const RingBuffer* rb) {
    if (rb == nullptr) {
        return false;
    }
    return rb->ready_count > 0;
}

uint8_t RingBuffer_GetReadyCount(const RingBuffer* rb) {
    if (rb == nullptr) {
        return 0;
    }
    return rb->ready_count;
}

uint8_t RingBuffer_GetAvailableCount(const RingBuffer* rb) {
    if (rb == nullptr) {
        return 0;
    }
    return RING_BUFFER_SIZE - rb->ready_count;
}

void RingBuffer_RecordProcessingTime(RingBuffer* rb, uint32_t time_ms) {
    if (rb == nullptr) {
        return;
    }
    
    rb->total_processing_time += time_ms;
    
    if (time_ms < rb->min_processing_time) {
        rb->min_processing_time = time_ms;
    }
    
    if (time_ms > rb->max_processing_time) {
        rb->max_processing_time = time_ms;
    }
}

uint32_t RingBuffer_GetAverageFPS(const RingBuffer* rb) {
    if (rb == nullptr || rb->runtime_us == 0) {
        return 0;
    }
    
    // 转换为秒
    uint64_t runtime_sec = rb->runtime_us / 1000000;
    if (runtime_sec == 0) {
        return 0;
    }
    
    return static_cast<uint32_t>(rb->total_frames / runtime_sec);
}

uint32_t RingBuffer_GetAverageProcessingTime(const RingBuffer* rb) {
    if (rb == nullptr || rb->total_frames == 0) {
        return 0;
    }
    
    return static_cast<uint32_t>(rb->total_processing_time / rb->total_frames);
}

uint32_t RingBuffer_GetDroppedFrameRate(const RingBuffer* rb) {
    if (rb == nullptr || rb->total_frames == 0) {
        return 0;
    }
    
    // 返回千分率
    return (rb->dropped_frames * 1000) / rb->total_frames;
}

void RingBuffer_UpdateRuntime(RingBuffer* rb, uint32_t delta_us) {
    if (rb == nullptr) {
        return;
    }
    
    rb->runtime_us += delta_us;
}

bool RingBuffer_CheckPerformanceDegradation(const RingBuffer* rb) {
    if (rb == nullptr || rb->total_frames < 100) {
        return false;
    }
    
    // 检查处理时间是否超过阈值
    uint32_t avg_time = RingBuffer_GetAverageProcessingTime(rb);
    
    // 如果平均处理时间超过帧间隔的 80%，认为性能衰减
    if (avg_time > FRAME_INTERVAL_MS * 8 / 10) {
        return true;
    }
    
    // 如果最大处理时间超过帧间隔，认为性能衰减
    if (rb->max_processing_time > FRAME_INTERVAL_MS) {
        return true;
    }
    
    // 如果丢帧率超过 5%，认为性能衰减
    uint32_t drop_rate = RingBuffer_GetDroppedFrameRate(rb);
    if (drop_rate > 50) { // 5% = 50 ‰
        return true;
    }
    
    return false;
}

void RingBuffer_RecordCheckpoint(RingBuffer* rb) {
    if (rb == nullptr) {
        return;
    }
    
    rb->runtime_checkpoints++;
    
    // 每小时（3600秒）记录一个检查点
    // 这里可以添加更多性能监控逻辑
}
