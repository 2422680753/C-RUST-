use crate::shared_types::*;

pub fn calculate_motion_direction(
    flow_result: &OpticalFlowResult,
    motion_threshold: u16
) -> MotionState {
    let mut state = MotionState::new();
    
    if flow_result.tracked_count < 5 {
        state.direction = MotionDirection::Unknown;
        state.confidence = 0;
        return state;
    }
    
    let mut total_dx: i32 = 0;
    let mut total_dy: i32 = 0;
    let mut valid_count: u16 = 0;
    
    let image_center_x = IMAGE_WIDTH as i32 / 2;
    let image_center_y = IMAGE_HEIGHT as i32 / 2;
    
    for i in 0..flow_result.tracked_count as usize {
        let corner = &flow_result.curr_corners[i];
        let dx = flow_result.dx[i] as i32;
        let dy = flow_result.dy[i] as i32;
        
        if dx.abs() < motion_threshold as i32 && dy.abs() < motion_threshold as i32 {
            continue;
        }
        
        let dist_from_center = (
            (corner.x as i32 - image_center_x).pow(2) + 
            (corner.y as i32 - image_center_y).pow(2)
        ) as f32;
        
        let max_dist = (image_center_x.pow(2) + image_center_y.pow(2)) as f32;
        let weight = 1.0 - (dist_from_center / max_dist).sqrt() * 0.5;
        
        total_dx += (dx as f32 * weight) as i32;
        total_dy += (dy as f32 * weight) as i32;
        valid_count += 1;
    }
    
    if valid_count < 3 {
        state.direction = MotionDirection::Stop;
        state.confidence = 20;
        return state;
    }
    
    let avg_dx = total_dx / valid_count as i32;
    let avg_dy = total_dy / valid_count as i32;
    
    let motion_magnitude = (avg_dx.abs() + avg_dy.abs()) as u16;
    
    if motion_magnitude < motion_threshold {
        state.direction = MotionDirection::Stop;
        state.confidence = 40;
        state.velocity_x = 0;
        state.velocity_y = 0;
        state.rotation = 0;
        return state;
    }
    
    let (direction, confidence) = analyze_direction(avg_dx, avg_dy, motion_threshold);
    
    let rotation = calculate_rotation(flow_result, avg_dx, avg_dy);
    
    let velocity_x = calculate_velocity_x(avg_dx);
    let velocity_y = calculate_velocity_y(avg_dy);
    
    state.direction = direction;
    state.velocity_x = velocity_x;
    state.velocity_y = velocity_y;
    state.rotation = rotation;
    state.confidence = confidence.min(100);
    
    state
}

fn analyze_direction(dx: i32, dy: i32, threshold: u16) -> (MotionDirection, u8) {
    let threshold_i32 = threshold as i32;
    
    let dx_abs = dx.abs();
    let dy_abs = dy.abs();
    
    let mut confidence: u8 = 50;
    
    let motion_strength = (dx_abs + dy_abs) as u16;
    if motion_strength > threshold * 3 {
        confidence += 30;
    } else if motion_strength > threshold * 2 {
        confidence += 15;
    }
    
    let dominant_x = dx_abs > dy_abs * 2;
    let dominant_y = dy_abs > dx_abs * 2;
    
    if dominant_y {
        if dy < -threshold_i32 {
            (MotionDirection::Forward, confidence + 10)
        } else if dy > threshold_i32 {
            (MotionDirection::Backward, confidence + 10)
        } else {
            (MotionDirection::Stop, confidence - 10)
        }
    } else if dominant_x {
        if dx < -threshold_i32 {
            (MotionDirection::Left, confidence + 10)
        } else if dx > threshold_i32 {
            (MotionDirection::Right, confidence + 10)
        } else {
            (MotionDirection::Stop, confidence - 10)
        }
    } else {
        if dy < -threshold_i32 {
            if dx < -threshold_i32 {
                (MotionDirection::Left, confidence)
            } else if dx > threshold_i32 {
                (MotionDirection::Right, confidence)
            } else {
                (MotionDirection::Forward, confidence)
            }
        } else if dy > threshold_i32 {
            (MotionDirection::Backward, confidence)
        } else if dx < -threshold_i32 {
            (MotionDirection::Left, confidence)
        } else if dx > threshold_i32 {
            (MotionDirection::Right, confidence)
        } else {
            (MotionDirection::Stop, confidence - 20)
        }
    }
}

fn calculate_rotation(flow_result: &OpticalFlowResult, avg_dx: i32, avg_dy: i32) -> i16 {
    if flow_result.tracked_count < 8 {
        return 0;
    }
    
    let image_center_x = IMAGE_WIDTH as f32 / 2.0;
    let image_center_y = IMAGE_HEIGHT as f32 / 2.0;
    
    let mut clockwise_count: u16 = 0;
    let mut counter_clockwise_count: u16 = 0;
    
    for i in 0..flow_result.tracked_count as usize {
        let corner = &flow_result.curr_corners[i];
        let dx = flow_result.dx[i] as f32;
        let dy = flow_result.dy[i] as f32;
        
        let rel_x = corner.x as f32 - image_center_x;
        let rel_y = corner.y as f32 - image_center_y;
        
        let cross_product = rel_x * dy - rel_y * dx;
        
        if cross_product > 5.0 {
            clockwise_count += 1;
        } else if cross_product < -5.0 {
            counter_clockwise_count += 1;
        }
    }
    
    let total_rotating = clockwise_count + counter_clockwise_count;
    if total_rotating < 4 {
        return 0;
    }
    
    let rotation_ratio = if clockwise_count > counter_clockwise_count {
        clockwise_count as f32 / total_rotating as f32
    } else {
        -(counter_clockwise_count as f32 / total_rotating as f32)
    };
    
    (rotation_ratio * 100.0) as i16
}

fn calculate_velocity_x(dx: i32) -> i16 {
    let speed = (dx.abs() * 10) as i16;
    speed.clamp(0, 1000)
}

fn calculate_velocity_y(dy: i32) -> i16 {
    let speed = (dy.abs() * 10) as i16;
    speed.clamp(0, 1000)
}

pub fn avoid_obstacles(
    motion_state: &MotionState,
    obstacles: &ObstacleDetectionResult,
    safe_distance: u16
) -> MotionState {
    let mut adjusted_state = *motion_state;
    
    if obstacles.count == 0 {
        return adjusted_state;
    }
    
    let image_center_x = IMAGE_WIDTH as i16 / 2;
    let image_center_y = IMAGE_HEIGHT as i16 / 2;
    
    let mut left_obstacle = false;
    let mut right_obstacle = false;
    let mut front_obstacle = false;
    
    for i in 0..obstacles.count as usize {
        let obstacle = &obstacles.obstacles[i];
        
        if obstacle.estimated_distance < safe_distance as i16 {
            if obstacle.center_x < image_center_x - 30 {
                left_obstacle = true;
            } else if obstacle.center_x > image_center_x + 30 {
                right_obstacle = true;
            }
            
            if obstacle.center_y < image_center_y {
                front_obstacle = true;
            }
        }
    }
    
    if front_obstacle {
        match adjusted_state.direction {
            MotionDirection::Forward => {
                if !left_obstacle && right_obstacle {
                    adjusted_state.direction = MotionDirection::Left;
                    adjusted_state.rotation = 50;
                    adjusted_state.confidence = adjusted_state.confidence.saturating_sub(10);
                } else if left_obstacle && !right_obstacle {
                    adjusted_state.direction = MotionDirection::Right;
                    adjusted_state.rotation = -50;
                    adjusted_state.confidence = adjusted_state.confidence.saturating_sub(10);
                } else if !left_obstacle && !right_obstacle {
                    adjusted_state.direction = MotionDirection::Left;
                    adjusted_state.rotation = 30;
                } else {
                    adjusted_state.direction = MotionDirection::Stop;
                    adjusted_state.velocity_x = 0;
                    adjusted_state.velocity_y = 0;
                }
            }
            MotionDirection::Left => {
                if left_obstacle {
                    adjusted_state.direction = MotionDirection::Stop;
                    adjusted_state.velocity_x = 0;
                }
            }
            MotionDirection::Right => {
                if right_obstacle {
                    adjusted_state.direction = MotionDirection::Stop;
                    adjusted_state.velocity_x = 0;
                }
            }
            _ => {}
        }
    }
    
    adjusted_state
}

pub fn merge_motion_decisions(
    optical_flow_direction: &MotionState,
    obstacle_avoidance_direction: &MotionState
) -> MotionState {
    if obstacle_avoidance_direction.direction == MotionDirection::Stop {
        return *obstacle_avoidance_direction;
    }
    
    if obstacle_avoidance_direction.direction != optical_flow_direction.direction {
        if obstacle_avoidance_direction.confidence > optical_flow_direction.confidence {
            return *obstacle_avoidance_direction;
        }
    }
    
    *optical_flow_direction
}
