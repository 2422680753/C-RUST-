#include "comm_manager.h"
#include <cstring>

// 静态共享内存（无堆内存分配）
static SharedMemory g_shared_memory;

// 通信管理器状态
static CommManagerState g_comm_state = {
    .current_state = COMM_STATE_IDLE,
    .last_frame_time_ms = 0,
    .frame_count = 0,
    .dropped_frames = 0,
    .total_processing_time_ms = 0,
    .average_cpu_usage = 0
};

// 回调函数
static FrameCompleteCallback g_frame_complete_callback = nullptr;
static ErrorCallback g_error_callback = nullptr;

// 简单的自旋锁（用于共享内存同步）
static volatile bool g_shared_memory_locked = false;

// 获取当前时间戳（毫秒）
static uint32_t GetCurrentTimeMs(void) {
    // 这里应该使用系统定时器
    // 暂时返回一个模拟值
    return g_comm_state.frame_count * FRAME_INTERVAL_MS;
}

void CommManager_Init(void) {
    // 初始化共享内存
    std::memset(&g_shared_memory, 0, sizeof(SharedMemory));
    
    g_shared_memory.new_frame_available = false;
    g_shared_memory.processing_complete = false;
    g_shared_memory.abort_processing = false;
    g_shared_memory.command_available = false;
    
    // 初始化状态
    g_comm_state.current_state = COMM_STATE_IDLE;
    g_comm_state.last_frame_time_ms = 0;
    g_comm_state.frame_count = 0;
    g_comm_state.dropped_frames = 0;
    g_comm_state.total_processing_time_ms = 0;
    g_comm_state.average_cpu_usage = 0;
    
    g_shared_memory_locked = false;
    
    // 调用 Rust 初始化函数
    rust_init();
}

void CommManager_DeInit(void) {
    // 停止所有处理
    g_shared_memory.abort_processing = true;
    
    // 重置状态
    g_comm_state.current_state = COMM_STATE_IDLE;
    
    // 清除回调
    g_frame_complete_callback = nullptr;
    g_error_callback = nullptr;
}

void CommManager_Start(void) {
    if (g_comm_state.current_state == COMM_STATE_IDLE) {
        g_comm_state.current_state = COMM_STATE_CAPTURING;
        g_comm_state.frame_count = 0;
        g_comm_state.dropped_frames = 0;
        g_comm_state.total_processing_time_ms = 0;
        g_comm_state.last_frame_time_ms = GetCurrentTimeMs();
    }
}

void CommManager_Stop(void) {
    g_shared_memory.abort_processing = true;
    g_comm_state.current_state = COMM_STATE_IDLE;
}

void CommManager_Reset(void) {
    CommManager_Stop();
    
    // 重置共享内存
    std::memset(&g_shared_memory, 0, sizeof(SharedMemory));
    g_shared_memory.new_frame_available = false;
    g_shared_memory.processing_complete = false;
    g_shared_memory.abort_processing = false;
    g_shared_memory.command_available = false;
    
    // 重置状态
    g_comm_state.current_state = COMM_STATE_IDLE;
    g_comm_state.last_frame_time_ms = 0;
    g_comm_state.frame_count = 0;
    g_comm_state.dropped_frames = 0;
    g_comm_state.total_processing_time_ms = 0;
    g_comm_state.average_cpu_usage = 0;
    
    // 重新初始化 Rust
    rust_init();
}

CommState CommManager_GetState(void) {
    return g_comm_state.current_state;
}

void CommManager_GetStateInfo(CommManagerState* state) {
    if (state == nullptr) {
        return;
    }
    
    *state = g_comm_state;
}

bool CommManager_SubmitFrame(const uint8_t* image_data, uint32_t timestamp_ms) {
    if (image_data == nullptr) {
        return false;
    }
    
    if (g_comm_state.current_state == COMM_STATE_IDLE) {
        return false;
    }
    
    // 检查是否正在处理上一帧
    if (g_comm_state.current_state == COMM_STATE_PROCESSING) {
        // 检查处理是否超时
        uint32_t current_time = GetCurrentTimeMs();
        if (current_time - g_comm_state.last_frame_time_ms > FRAME_INTERVAL_MS * 2) {
            // 超时，增加丢帧计数
            g_comm_state.dropped_frames++;
            g_shared_memory.abort_processing = true;
            g_comm_state.current_state = COMM_STATE_CAPTURING;
        } else {
            // 正在处理，丢帧
            g_comm_state.dropped_frames++;
            return false;
        }
    }
    
    // 锁定共享内存
    CommManager_LockSharedMemory();
    
    // 复制上一帧到 previous_frame
    std::memcpy(&g_shared_memory.previous_frame, &g_shared_memory.current_frame, sizeof(ImageFrame));
    
    // 复制新帧到 current_frame
    std::memcpy(g_shared_memory.current_frame.data, image_data, IMAGE_SIZE);
    g_shared_memory.current_frame.timestamp_ms = timestamp_ms;
    g_shared_memory.current_frame.is_valid = true;
    
    // 设置新帧标志
    g_shared_memory.new_frame_available = true;
    g_shared_memory.processing_complete = false;
    
    // 更新状态
    g_comm_state.current_state = COMM_STATE_PROCESSING;
    g_comm_state.last_frame_time_ms = timestamp_ms;
    
    // 解锁共享内存
    CommManager_UnlockSharedMemory();
    
    // 调用 Rust 处理函数（直接调用，零拷贝）
    rust_process_frame(
        &g_shared_memory.current_frame,
        &g_shared_memory.previous_frame,
        &g_shared_memory.processing_output
    );
    
    // 处理完成
    g_shared_memory.processing_complete = true;
    g_shared_memory.new_frame_available = false;
    
    // 更新统计
    g_comm_state.frame_count++;
    g_comm_state.total_processing_time_ms += g_shared_memory.processing_output.processing_time_ms;
    
    if (g_comm_state.frame_count > 0) {
        g_comm_state.average_cpu_usage = static_cast<uint8_t>(
            (g_comm_state.total_processing_time_ms * 100) / 
            (g_comm_state.frame_count * FRAME_INTERVAL_MS)
        );
    }
    
    g_comm_state.current_state = COMM_STATE_COMPLETE;
    
    // 调用回调
    if (g_frame_complete_callback != nullptr) {
        g_frame_complete_callback(&g_shared_memory.processing_output);
    }
    
    return true;
}

bool CommManager_HasNewFrame(void) {
    return g_shared_memory.new_frame_available;
}

bool CommManager_IsProcessingComplete(void) {
    return g_shared_memory.processing_complete;
}

bool CommManager_GetProcessingOutput(ImageProcessingOutput* output) {
    if (output == nullptr) {
        return false;
    }
    
    if (!g_shared_memory.processing_complete) {
        return false;
    }
    
    CommManager_LockSharedMemory();
    *output = g_shared_memory.processing_output;
    CommManager_UnlockSharedMemory();
    
    return true;
}

bool CommManager_GetObstacleResult(ObstacleDetectionResult* result) {
    if (result == nullptr) {
        return false;
    }
    
    if (!g_shared_memory.processing_complete) {
        return false;
    }
    
    CommManager_LockSharedMemory();
    *result = g_shared_memory.processing_output.obstacle_result;
    CommManager_UnlockSharedMemory();
    
    return true;
}

bool CommManager_GetMotionState(MotionState* state) {
    if (state == nullptr) {
        return false;
    }
    
    if (!g_shared_memory.processing_complete) {
        return false;
    }
    
    CommManager_LockSharedMemory();
    *state = g_shared_memory.processing_output.motion_state;
    CommManager_UnlockSharedMemory();
    
    return true;
}

SharedMemory* CommManager_GetSharedMemory(void) {
    return &g_shared_memory;
}

void CommManager_LockSharedMemory(void) {
    // 简单的自旋锁实现
    // 在实际嵌入式系统中应该使用互斥锁或临界区
    while (g_shared_memory_locked) {
        // 等待
    }
    g_shared_memory_locked = true;
}

void CommManager_UnlockSharedMemory(void) {
    g_shared_memory_locked = false;
}

uint32_t CommManager_GetCurrentFPS(void) {
    if (g_comm_state.frame_count == 0) {
        return 0;
    }
    
    uint32_t current_time = GetCurrentTimeMs();
    uint32_t elapsed = current_time - g_comm_state.last_frame_time_ms;
    
    if (elapsed == 0) {
        return TARGET_FPS;
    }
    
    return (g_comm_state.frame_count * 1000) / elapsed;
}

uint32_t CommManager_GetAverageProcessingTime(void) {
    if (g_comm_state.frame_count == 0) {
        return 0;
    }
    
    return g_comm_state.total_processing_time_ms / g_comm_state.frame_count;
}

uint8_t CommManager_GetEstimatedCPUUsage(void) {
    return g_comm_state.average_cpu_usage;
}

void CommManager_ExecuteDebugCommand(const DebugCommand* command) {
    if (command == nullptr) {
        return;
    }
    
    switch (command->type) {
        case DEBUG_CMD_START:
            CommManager_Start();
            break;
            
        case DEBUG_CMD_STOP:
            CommManager_Stop();
            break;
            
        case DEBUG_CMD_RESET:
            CommManager_Reset();
            break;
            
        case DEBUG_CMD_SET_SPEED:
            // 这里可以添加设置速度的逻辑
            break;
            
        case DEBUG_CMD_GET_STATUS:
            // 状态获取通过 GetSystemStatus 实现
            break;
            
        case DEBUG_CMD_SET_PARAM:
            rust_set_parameter(
                static_cast<DebugParameter>(command->param1),
                command->param2
            );
            break;
            
        case DEBUG_CMD_GET_PARAM:
            // 参数获取通过 rust_get_parameter 实现
            break;
            
        default:
            break;
    }
}

void CommManager_GetSystemStatus(SystemStatus* status) {
    if (status == nullptr) {
        return;
    }
    
    rust_get_status(status);
    
    // 补充 C++ 端的状态
    status->frame_count = g_comm_state.frame_count;
    status->fps_actual = CommManager_GetCurrentFPS();
    status->total_processing_time_ms = g_comm_state.total_processing_time_ms;
    status->average_cpu_usage = g_comm_state.average_cpu_usage;
}

void CommManager_RegisterFrameCompleteCallback(FrameCompleteCallback callback) {
    g_frame_complete_callback = callback;
}

void CommManager_RegisterErrorCallback(ErrorCallback callback) {
    g_error_callback = callback;
}
