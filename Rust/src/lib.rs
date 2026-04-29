#![no_std]
#![no_main]

mod image_proc;
mod feature;
mod obstacle;
mod motion;
mod shared_types;

use core::panic::PanicInfo;
use crate::shared_types::*;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

static mut PROCESSING_OUTPUT: ImageProcessingOutput = ImageProcessingOutput::new();
static mut SYSTEM_STATUS: SystemStatus = SystemStatus::new();

static mut FAST_THRESHOLD: u8 = 20;
static mut MIN_CORNER_DISTANCE: u8 = 10;
static mut MAX_CORNERS: u16 = 200;
static mut OBSTACLE_MIN_SIZE: u16 = 20;
static mut OBSTACLE_MAX_SIZE: u16 = 100;
static mut MOTION_THRESHOLD: u16 = 5;

#[no_mangle]
pub extern "C" fn rust_init() {
    unsafe {
        PROCESSING_OUTPUT = ImageProcessingOutput::new();
        SYSTEM_STATUS = SystemStatus::new();
        
        FAST_THRESHOLD = 20;
        MIN_CORNER_DISTANCE = 10;
        MAX_CORNERS = 200;
        OBSTACLE_MIN_SIZE = 20;
        OBSTACLE_MAX_SIZE = 100;
        MOTION_THRESHOLD = 5;
    }
}

#[no_mangle]
pub extern "C" fn rust_process_frame(
    current: *const ImageFrame,
    previous: *const ImageFrame,
    output: *mut ImageProcessingOutput
) {
    if current.is_null() || previous.is_null() || output.is_null() {
        return;
    }
    
    unsafe {
        let current_frame = &*current;
        let prev_frame = &*previous;
        
        if !current_frame.is_valid || !prev_frame.is_valid {
            (*output).is_valid = false;
            return;
        }
        
        let start_time = current_frame.timestamp_ms;
        
        let corners = feature::fast_detect(
            &current_frame.data,
            FAST_THRESHOLD,
            MAX_CORNERS,
            MIN_CORNER_DISTANCE
        );
        
        let flow_result = feature::optical_flow_tracking(
            &prev_frame.data,
            &current_frame.data,
            &corners,
            MAX_CORNERS
        );
        
        let obstacle_result = obstacle::detect_obstacles(
            &flow_result,
            OBSTACLE_MIN_SIZE,
            OBSTACLE_MAX_SIZE
        );
        
        let motion_state = motion::calculate_motion_direction(
            &flow_result,
            MOTION_THRESHOLD
        );
        
        let processing_time = current_frame.timestamp_ms.saturating_sub(start_time);
        
        (*output).obstacle_result = obstacle_result;
        (*output).motion_state = motion_state;
        (*output).processing_time_ms = processing_time;
        (*output).cpu_usage_estimate = estimate_cpu_usage(processing_time);
        (*output).is_valid = true;
        
        SYSTEM_STATUS.frame_count += 1;
        SYSTEM_STATUS.total_processing_time_ms += processing_time;
        SYSTEM_STATUS.obstacle_count = obstacle_result.count;
        SYSTEM_STATUS.current_direction = motion_state.direction;
        SYSTEM_STATUS.uptime_ms = current_frame.timestamp_ms;
        
        if SYSTEM_STATUS.frame_count % 10 == 0 {
            SYSTEM_STATUS.fps_actual = calculate_actual_fps();
            SYSTEM_STATUS.average_cpu_usage = (SYSTEM_STATUS.total_processing_time_ms / 
                                              (SYSTEM_STATUS.frame_count * FRAME_INTERVAL_MS)) as u8;
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_set_parameter(param: DebugParameter, value: i32) {
    unsafe {
        match param {
            DebugParameter::FastThreshold => {
                if value >= 0 && value <= 255 {
                    FAST_THRESHOLD = value as u8;
                }
            }
            DebugParameter::MinCornerDistance => {
                if value >= 1 && value <= 50 {
                    MIN_CORNER_DISTANCE = value as u8;
                }
            }
            DebugParameter::MaxCorners => {
                if value >= 10 && value <= 500 {
                    MAX_CORNERS = value as u16;
                }
            }
            DebugParameter::ObstacleMinSize => {
                if value >= 10 && value <= 100 {
                    OBSTACLE_MIN_SIZE = value as u16;
                }
            }
            DebugParameter::ObstacleMaxSize => {
                if value >= 50 && value <= 200 {
                    OBSTACLE_MAX_SIZE = value as u16;
                }
            }
            DebugParameter::MotionThreshold => {
                if value >= 1 && value <= 20 {
                    MOTION_THRESHOLD = value as u16;
                }
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_get_parameter(param: DebugParameter) -> i32 {
    unsafe {
        match param {
            DebugParameter::FastThreshold => FAST_THRESHOLD as i32,
            DebugParameter::MinCornerDistance => MIN_CORNER_DISTANCE as i32,
            DebugParameter::MaxCorners => MAX_CORNERS as i32,
            DebugParameter::ObstacleMinSize => OBSTACLE_MIN_SIZE as i32,
            DebugParameter::ObstacleMaxSize => OBSTACLE_MAX_SIZE as i32,
            DebugParameter::MotionThreshold => MOTION_THRESHOLD as i32,
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_get_status(status: *mut SystemStatus) {
    if status.is_null() {
        return;
    }
    
    unsafe {
        *status = SYSTEM_STATUS;
    }
}

fn estimate_cpu_usage(processing_time_ms: u32) -> u8 {
    let usage = (processing_time_ms * 100) / FRAME_INTERVAL_MS;
    if usage > 100 {
        100
    } else {
        usage as u8
    }
}

fn calculate_actual_fps() -> u32 {
    if SYSTEM_STATUS.frame_count == 0 {
        return 0;
    }
    
    let total_time = SYSTEM_STATUS.uptime_ms;
    if total_time == 0 {
        return 0;
    }
    
    (SYSTEM_STATUS.frame_count * 1000) / total_time
}
