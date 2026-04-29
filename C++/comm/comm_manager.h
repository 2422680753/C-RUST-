#ifndef COMM_MANAGER_H
#define COMM_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "../../include/shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// 通信状态枚举
typedef enum {
    COMM_STATE_IDLE = 0,
    COMM_STATE_CAPTURING = 1,
    COMM_STATE_PROCESSING = 2,
    COMM_STATE_COMPLETE = 3,
    COMM_STATE_ERROR = 4
} CommState;

// 通信管理器状态
typedef struct {
    CommState current_state;
    uint32_t last_frame_time_ms;
    uint32_t frame_count;
    uint32_t dropped_frames;
    uint32_t total_processing_time_ms;
    uint8_t average_cpu_usage;
} CommManagerState;

// 通信管理器函数声明
void CommManager_Init(void);
void CommManager_DeInit(void);

void CommManager_Start(void);
void CommManager_Stop(void);
void CommManager_Reset(void);

CommState CommManager_GetState(void);
void CommManager_GetStateInfo(CommManagerState* state);

// 帧管理函数
bool CommManager_SubmitFrame(const uint8_t* image_data, uint32_t timestamp_ms);
bool CommManager_HasNewFrame(void);
bool CommManager_IsProcessingComplete(void);

// 结果获取函数
bool CommManager_GetProcessingOutput(ImageProcessingOutput* output);
bool CommManager_GetObstacleResult(ObstacleDetectionResult* result);
bool CommManager_GetMotionState(MotionState* state);

// 共享内存访问函数
SharedMemory* CommManager_GetSharedMemory(void);
void CommManager_LockSharedMemory(void);
void CommManager_UnlockSharedMemory(void);

// 性能监控函数
uint32_t CommManager_GetCurrentFPS(void);
uint32_t CommManager_GetAverageProcessingTime(void);
uint8_t CommManager_GetEstimatedCPUUsage(void);

// 调试接口
void CommManager_ExecuteDebugCommand(const DebugCommand* command);
void CommManager_GetSystemStatus(SystemStatus* status);

// 回调函数注册
typedef void (*FrameCompleteCallback)(const ImageProcessingOutput* output);
typedef void (*ErrorCallback)(uint32_t error_code);

void CommManager_RegisterFrameCompleteCallback(FrameCompleteCallback callback);
void CommManager_RegisterErrorCallback(ErrorCallback callback);

#ifdef __cplusplus
}
#endif

#endif // COMM_MANAGER_H
