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
        unsafe {
            if !output.is_null() {
                (*output).is_valid = false;
            }
        }
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

#[no_mangle]
pub extern "C" fn rust_detect_obstacles_from_frames(
    prev_data: *const u8,
    curr_data: *const u8,
    obstacles: *mut ObstacleDetectionResult
) {
    if prev_data.is_null() || curr_data.is_null() || obstacles.is_null() {
        return;
    }
    
    unsafe {
        let prev_slice = core::slice::from_raw_parts(prev_data, IMAGE_SIZE);
        let curr_slice = core::slice::from_raw_parts(curr_data, IMAGE_SIZE);
        
        let corners = feature::fast_detect(
            curr_slice,
            FAST_THRESHOLD,
            MAX_CORNERS,
            MIN_CORNER_DISTANCE
        );
        
        let flow_result = feature::optical_flow_tracking(
            prev_slice,
            curr_slice,
            &corners,
            MAX_CORNERS
        );
        
        let obstacle_result = obstacle::detect_obstacles(
            &flow_result,
            OBSTACLE_MIN_SIZE,
            OBSTACLE_MAX_SIZE
        );
        
        *obstacles = obstacle_result;
    }
}

#[no_mangle]
pub extern "C" fn rust_calculate_motion(
    flow_result: *const OpticalFlowResult,
    motion_state: *mut MotionState
) {
    if flow_result.is_null() || motion_state.is_null() {
        return;
    }
    
    unsafe {
        let flow = &*flow_result;
        let state = motion::calculate_motion_direction(flow, MOTION_THRESHOLD);
        *motion_state = state;
    }
}

#[no_mangle]
pub extern "C" fn rust_fast_detect_corners(
    image_data: *const u8,
    threshold: u8,
    max_corners: u16,
    min_distance: u8,
    result: *mut CornerDetectionResult
) {
    if image_data.is_null() || result.is_null() {
        return;
    }
    
    unsafe {
        let slice = core::slice::from_raw_parts(image_data, IMAGE_SIZE);
        let corners = feature::fast_detect(
            slice,
            threshold,
            max_corners,
            min_distance
        );
        *result = corners;
    }
}

#[no_mangle]
pub extern "C" fn rust_optical_flow_track(
    prev_image: *const u8,
    curr_image: *const u8,
    prev_corners: *const CornerDetectionResult,
    max_corners: u16,
    result: *mut OpticalFlowResult
) {
    if prev_image.is_null() || curr_image.is_null() || 
       prev_corners.is_null() || result.is_null() {
        return;
    }
    
    unsafe {
        let prev_slice = core::slice::from_raw_parts(prev_image, IMAGE_SIZE);
        let curr_slice = core::slice::from_raw_parts(curr_image, IMAGE_SIZE);
        let corners = &*prev_corners;
        
        let flow = feature::optical_flow_tracking(
            prev_slice,
            curr_slice,
            corners,
            max_corners
        );
        *result = flow;
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
    unsafe {
        if SYSTEM_STATUS.frame_count == 0 {
            return 0;
        }
        
        let total_time = SYSTEM_STATUS.uptime_ms;
        if total_time == 0 {
            return 0;
        }
        
        (SYSTEM_STATUS.frame_count * 1000) / total_time
    }
}

#[no_mangle]
pub extern "C" fn rust_get_version_major() -> u8 {
    0
}

#[no_mangle]
pub extern "C" fn rust_get_version_minor() -> u8 {
    1
}

#[no_mangle]
pub extern "C" fn rust_get_version_patch() -> u8 {
    0
}

#[no_mangle]
pub extern "C" fn rust_is_initialized() -> bool {
    unsafe {
        SYSTEM_STATUS.frame_count > 0 || 
        FAST_THRESHOLD != 0
    }
}

#[no_mangle]
pub extern "C" fn rust_reset() {
    unsafe {
        PROCESSING_OUTPUT = ImageProcessingOutput::new();
        SYSTEM_STATUS = SystemStatus::new();
    }
}

#[no_mangle]
pub extern "C" fn rust_get_frame_count() -> u32 {
    unsafe {
        SYSTEM_STATUS.frame_count
    }
}

#[no_mangle]
pub extern "C" fn rust_get_average_cpu_usage() -> u8 {
    unsafe {
        SYSTEM_STATUS.average_cpu_usage
    }
}

#[no_mangle]
pub extern "C" fn rust_get_current_fps() -> u32 {
    unsafe {
        SYSTEM_STATUS.fps_actual
    }
}
