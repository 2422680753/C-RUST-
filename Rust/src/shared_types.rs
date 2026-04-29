use core::default::Default;

pub const IMAGE_WIDTH: u16 = 320;
pub const IMAGE_HEIGHT: u16 = 240;
pub const IMAGE_SIZE: usize = (IMAGE_WIDTH as usize) * (IMAGE_HEIGHT as usize);

pub const MAX_CORNERS: usize = 200;
pub const MAX_OBSTACLES: usize = 10;

pub const TARGET_FPS: u32 = 15;
pub const FRAME_INTERVAL_MS: u32 = 1000 / TARGET_FPS;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ImageFrame {
    pub data: [u8; IMAGE_SIZE],
    pub timestamp_ms: u32,
    pub is_valid: bool,
}

impl ImageFrame {
    pub const fn new() -> Self {
        Self {
            data: [0; IMAGE_SIZE],
            timestamp_ms: 0,
            is_valid: false,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Corner {
    pub x: i16,
    pub y: i16,
    pub response: u8,
    pub reserved: u8,
}

impl Corner {
    pub const fn new() -> Self {
        Self {
            x: 0,
            y: 0,
            response: 0,
            reserved: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CornerDetectionResult {
    pub corners: [Corner; MAX_CORNERS],
    pub count: u16,
    pub timestamp_ms: u32,
}

impl Default for CornerDetectionResult {
    fn default() -> Self {
        Self::new()
    }
}

impl CornerDetectionResult {
    pub const fn new() -> Self {
        Self {
            corners: [Corner::new(); MAX_CORNERS],
            count: 0,
            timestamp_ms: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OpticalFlowResult {
    pub prev_corners: [Corner; MAX_CORNERS],
    pub curr_corners: [Corner; MAX_CORNERS],
    pub dx: [i16; MAX_CORNERS],
    pub dy: [i16; MAX_CORNERS],
    pub tracked_count: u16,
    pub timestamp_ms: u32,
}

impl Default for OpticalFlowResult {
    fn default() -> Self {
        Self::new()
    }
}

impl OpticalFlowResult {
    pub const fn new() -> Self {
        Self {
            prev_corners: [Corner::new(); MAX_CORNERS],
            curr_corners: [Corner::new(); MAX_CORNERS],
            dx: [0; MAX_CORNERS],
            dy: [0; MAX_CORNERS],
            tracked_count: 0,
            timestamp_ms: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Obstacle {
    pub center_x: i16,
    pub center_y: i16,
    pub width: i16,
    pub height: i16,
    pub estimated_distance: i16,
    pub motion_dx: i16,
    pub motion_dy: i16,
    pub confidence: u8,
    pub reserved: u8,
}

impl Obstacle {
    pub const fn new() -> Self {
        Self {
            center_x: 0,
            center_y: 0,
            width: 0,
            height: 0,
            estimated_distance: 0,
            motion_dx: 0,
            motion_dy: 0,
            confidence: 0,
            reserved: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ObstacleDetectionResult {
    pub obstacles: [Obstacle; MAX_OBSTACLES],
    pub count: u16,
    pub timestamp_ms: u32,
}

impl Default for ObstacleDetectionResult {
    fn default() -> Self {
        Self::new()
    }
}

impl ObstacleDetectionResult {
    pub const fn new() -> Self {
        Self {
            obstacles: [Obstacle::new(); MAX_OBSTACLES],
            count: 0,
            timestamp_ms: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum MotionDirection {
    Forward = 0,
    Backward = 1,
    Left = 2,
    Right = 3,
    Stop = 4,
    Unknown = 5,
}

impl Default for MotionDirection {
    fn default() -> Self {
        MotionDirection::Unknown
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct MotionState {
    pub direction: MotionDirection,
    pub velocity_x: i16,
    pub velocity_y: i16,
    pub rotation: i16,
    pub confidence: u8,
    pub reserved: u8,
}

impl MotionState {
    pub const fn new() -> Self {
        Self {
            direction: MotionDirection::Unknown,
            velocity_x: 0,
            velocity_y: 0,
            rotation: 0,
            confidence: 0,
            reserved: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ImageProcessingOutput {
    pub obstacle_result: ObstacleDetectionResult,
    pub motion_state: MotionState,
    pub processing_time_ms: u32,
    pub cpu_usage_estimate: u8,
    pub is_valid: bool,
}

impl ImageProcessingOutput {
    pub const fn new() -> Self {
        Self {
            obstacle_result: ObstacleDetectionResult::new(),
            motion_state: MotionState::new(),
            processing_time_ms: 0,
            cpu_usage_estimate: 0,
            is_valid: false,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct MotorControlState {
    pub left_speed: i16,
    pub right_speed: i16,
    pub left_forward: bool,
    pub right_forward: bool,
    pub timestamp_ms: u32,
}

impl MotorControlState {
    pub const fn new() -> Self {
        Self {
            left_speed: 0,
            right_speed: 0,
            left_forward: false,
            right_forward: false,
            timestamp_ms: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum DebugCommandType {
    Start = 0,
    Stop = 1,
    Reset = 2,
    SetSpeed = 3,
    GetStatus = 4,
    SetParam = 5,
    GetParam = 6,
}

impl Default for DebugCommandType {
    fn default() -> Self {
        DebugCommandType::GetStatus
    }
}

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum DebugParameter {
    FastThreshold = 0,
    MinCornerDistance = 1,
    MaxCorners = 2,
    ObstacleMinSize = 3,
    ObstacleMaxSize = 4,
    MotionThreshold = 5,
}

impl Default for DebugParameter {
    fn default() -> Self {
        DebugParameter::FastThreshold
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DebugCommand {
    pub command_type: DebugCommandType,
    pub param1: u32,
    pub param2: i32,
    pub checksum: u8,
}

impl DebugCommand {
    pub const fn new() -> Self {
        Self {
            command_type: DebugCommandType::GetStatus,
            param1: 0,
            param2: 0,
            checksum: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SystemStatus {
    pub frame_count: u32,
    pub fps_actual: u32,
    pub total_processing_time_ms: u32,
    pub average_cpu_usage: u8,
    pub obstacle_count: u8,
    pub current_direction: MotionDirection,
    pub uptime_ms: u32,
}

impl SystemStatus {
    pub const fn new() -> Self {
        Self {
            frame_count: 0,
            fps_actual: 0,
            total_processing_time_ms: 0,
            average_cpu_usage: 0,
            obstacle_count: 0,
            current_direction: MotionDirection::Unknown,
            uptime_ms: 0,
        }
    }
}

#[repr(C)]
pub struct SharedMemory {
    pub current_frame: ImageFrame,
    pub previous_frame: ImageFrame,
    pub processing_output: ImageProcessingOutput,
    pub new_frame_available: bool,
    pub processing_complete: bool,
    pub abort_processing: bool,
    pub motor_state: MotorControlState,
    pub pending_command: DebugCommand,
    pub command_available: bool,
    pub system_status: SystemStatus,
}

impl SharedMemory {
    pub const fn new() -> Self {
        Self {
            current_frame: ImageFrame::new(),
            previous_frame: ImageFrame::new(),
            processing_output: ImageProcessingOutput::new(),
            new_frame_available: false,
            processing_complete: false,
            abort_processing: false,
            motor_state: MotorControlState::new(),
            pending_command: DebugCommand::new(),
            command_available: false,
            system_status: SystemStatus::new(),
        }
    }
}
