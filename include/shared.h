#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 图像分辨率定义
#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240
#define IMAGE_SIZE (IMAGE_WIDTH * IMAGE_HEIGHT)

// 最大角点数量
#define MAX_CORNERS 200

// 最大障碍物数量
#define MAX_OBSTACLES 10

// 帧率要求
#define TARGET_FPS 15
#define FRAME_INTERVAL_MS (1000 / TARGET_FPS)

// 图像帧结构 - 静态内存分配
typedef struct {
    uint8_t data[IMAGE_SIZE];
    uint32_t timestamp_ms;
    bool is_valid;
} ImageFrame;

// 角点结构
typedef struct {
    int16_t x;
    int16_t y;
    uint8_t response;
    uint8_t reserved;
} Corner;

// 角点检测结果
typedef struct {
    Corner corners[MAX_CORNERS];
    uint16_t count;
    uint32_t timestamp_ms;
} CornerDetectionResult;

// 光流跟踪结果
typedef struct {
    Corner prev_corners[MAX_CORNERS];
    Corner curr_corners[MAX_CORNERS];
    int16_t dx[MAX_CORNERS];
    int16_t dy[MAX_CORNERS];
    uint16_t tracked_count;
    uint32_t timestamp_ms;
} OpticalFlowResult;

// 障碍物信息
typedef struct {
    int16_t center_x;
    int16_t center_y;
    int16_t width;
    int16_t height;
    int16_t estimated_distance;
    int16_t motion_dx;
    int16_t motion_dy;
    uint8_t confidence;
    uint8_t reserved;
} Obstacle;

// 障碍物检测结果
typedef struct {
    Obstacle obstacles[MAX_OBSTACLES];
    uint16_t count;
    uint32_t timestamp_ms;
} ObstacleDetectionResult;

// 运动方向
typedef enum {
    DIRECTION_FORWARD = 0,
    DIRECTION_BACKWARD = 1,
    DIRECTION_LEFT = 2,
    DIRECTION_RIGHT = 3,
    DIRECTION_STOP = 4,
    DIRECTION_UNKNOWN = 5
} MotionDirection;

// 运动状态
typedef struct {
    MotionDirection direction;
    int16_t velocity_x;
    int16_t velocity_y;
    int16_t rotation;
    uint8_t confidence;
    uint8_t reserved;
} MotionState;

// 图像处理输出
typedef struct {
    ObstacleDetectionResult obstacle_result;
    MotionState motion_state;
    uint32_t processing_time_ms;
    uint8_t cpu_usage_estimate;
    bool is_valid;
} ImageProcessingOutput;

// 电机控制状态
typedef struct {
    int16_t left_speed;
    int16_t right_speed;
    bool left_forward;
    bool right_forward;
    uint32_t timestamp_ms;
} MotorControlState;

// 调试命令
typedef enum {
    DEBUG_CMD_START = 0,
    DEBUG_CMD_STOP = 1,
    DEBUG_CMD_RESET = 2,
    DEBUG_CMD_SET_SPEED = 3,
    DEBUG_CMD_GET_STATUS = 4,
    DEBUG_CMD_SET_PARAM = 5,
    DEBUG_CMD_GET_PARAM = 6
} DebugCommandType;

// 调试参数
typedef enum {
    DEBUG_PARAM_FAST_THRESHOLD = 0,
    DEBUG_PARAM_MIN_CORNER_DISTANCE = 1,
    DEBUG_PARAM_MAX_CORNERS = 2,
    DEBUG_PARAM_OBSTACLE_MIN_SIZE = 3,
    DEBUG_PARAM_OBSTACLE_MAX_SIZE = 4,
    DEBUG_PARAM_MOTION_THRESHOLD = 5
} DebugParameter;

// 调试命令
typedef struct {
    DebugCommandType type;
    uint32_t param1;
    int32_t param2;
    uint8_t checksum;
} DebugCommand;

// 系统状态
typedef struct {
    uint32_t frame_count;
    uint32_t fps_actual;
    uint32_t total_processing_time_ms;
    uint8_t average_cpu_usage;
    uint8_t obstacle_count;
    MotionDirection current_direction;
    uint32_t uptime_ms;
} SystemStatus;

// 共享内存结构 - 用于 C++ 和 Rust 之间的通信
typedef struct {
    // 输入数据（由 C++ 填充，Rust 读取）
    ImageFrame current_frame;
    ImageFrame previous_frame;
    
    // 输出数据（由 Rust 填充，C++ 读取）
    ImageProcessingOutput processing_output;
    
    // 控制信号
    bool new_frame_available;
    bool processing_complete;
    bool abort_processing;
    
    // 电机控制状态（由 C++ 填充，Rust 可读取）
    MotorControlState motor_state;
    
    // 调试命令和状态
    DebugCommand pending_command;
    bool command_available;
    SystemStatus system_status;
} SharedMemory;

// Rust 函数声明 - 由 Rust 实现，C++ 调用
void rust_init(void);
void rust_process_frame(const ImageFrame* current, const ImageFrame* previous, 
                        ImageProcessingOutput* output);
void rust_set_parameter(DebugParameter param, int32_t value);
int32_t rust_get_parameter(DebugParameter param);
void rust_get_status(SystemStatus* status);

// C++ 回调函数声明 - 由 C++ 实现，Rust 可调用
void cpp_log_message(const char* message);
void cpp_send_uart(const uint8_t* data, uint16_t length);
void cpp_get_motor_state(MotorControlState* state);

#ifdef __cplusplus
}
#endif

#endif // SHARED_H
