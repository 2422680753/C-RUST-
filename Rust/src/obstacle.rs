use crate::shared_types::*;

const GRID_SIZE: u16 = 40;
const MIN_CLUSTER_SIZE: u16 = 3;

pub fn detect_obstacles(
    flow_result: &OpticalFlowResult,
    min_size: u16,
    max_size: u16
) -> ObstacleDetectionResult {
    let mut result = ObstacleDetectionResult::new();
    let mut obstacle_count: u16 = 0;
    
    if flow_result.tracked_count < 5 {
        return result;
    }
    
    let grid_cols = (IMAGE_WIDTH + GRID_SIZE - 1) / GRID_SIZE;
    let grid_rows = (IMAGE_HEIGHT + GRID_SIZE - 1) / GRID_SIZE;
    
    let mut grid_motion = [[(0i32, 0i32, 0u16); 6]; 8];
    
    for i in 0..flow_result.tracked_count as usize {
        let corner = &flow_result.curr_corners[i];
        let dx = flow_result.dx[i] as i32;
        let dy = flow_result.dy[i] as i32;
        
        let grid_x = (corner.x as u16) / GRID_SIZE;
        let grid_y = (corner.y as u16) / GRID_SIZE;
        
        if grid_x < grid_cols && grid_y < grid_rows {
            let gx = grid_x as usize;
            let gy = grid_y as usize;
            
            grid_motion[gy][gx].0 += dx;
            grid_motion[gy][gx].1 += dy;
            grid_motion[gy][gx].2 += 1;
        }
    }
    
    let mut global_dx = 0i32;
    let mut global_dy = 0i32;
    let mut global_count = 0u16;
    
    for gy in 0..grid_rows as usize {
        for gx in 0..grid_cols as usize {
            if grid_motion[gy][gx].2 >= 2 {
                global_dx += grid_motion[gy][gx].0;
                global_dy += grid_motion[gy][gx].1;
                global_count += grid_motion[gy][gx].2;
            }
        }
    }
    
    let avg_dx = if global_count > 0 {
        global_dx / global_count as i32
    } else {
        0
    };
    
    let avg_dy = if global_count > 0 {
        global_dy / global_count as i32
    } else {
        0
    };
    
    let mut clusters: [(i32, i32, u16, i16, i16); MAX_OBSTACLES] = [(0, 0, 0, 0, 0); MAX_OBSTACLES];
    let mut cluster_count: u16 = 0;
    
    for i in 0..flow_result.tracked_count as usize {
        let corner = &flow_result.curr_corners[i];
        let dx = flow_result.dx[i] as i32;
        let dy = flow_result.dy[i] as i32;
        
        let rel_dx = dx - avg_dx;
        let rel_dy = dy - avg_dy;
        
        let motion_magnitude = (rel_dx.abs() + rel_dy.abs()) as u16;
        
        if motion_magnitude < 3 {
            continue;
        }
        
        let mut added_to_cluster = false;
        
        for c in 0..cluster_count as usize {
            let (cx, cy, count, _, _) = clusters[c];
            
            let dist_x = (corner.x as i32 - cx).abs();
            let dist_y = (corner.y as i32 - cy).abs();
            
            if dist_x < (min_size as i32) && dist_y < (min_size as i32) {
                clusters[c].0 = (cx * count as i32 + corner.x as i32) / (count + 1) as i32;
                clusters[c].1 = (cy * count as i32 + corner.y as i32) / (count + 1) as i32;
                clusters[c].2 += 1;
                clusters[c].3 = (clusters[c].3 * count as i16 + rel_dx as i16) / (count + 1) as i16;
                clusters[c].4 = (clusters[c].4 * count as i16 + rel_dy as i16) / (count + 1) as i16;
                
                added_to_cluster = true;
                break;
            }
        }
        
        if !added_to_cluster && cluster_count < MAX_OBSTACLES as u16 {
            clusters[cluster_count as usize] = (
                corner.x as i32,
                corner.y as i32,
                1,
                rel_dx as i16,
                rel_dy as i16
            );
            cluster_count += 1;
        }
    }
    
    let mut merged_clusters: [(i32, i32, u16, i16, i16); MAX_OBSTACLES] = [(0, 0, 0, 0, 0); MAX_OBSTACLES];
    let mut merged_count: u16 = 0;
    
    for c in 0..cluster_count as usize {
        let (cx, cy, count, mdx, mdy) = clusters[c];
        
        if count < MIN_CLUSTER_SIZE {
            continue;
        }
        
        let size = (count as u16 * 5).min(max_size);
        
        let mut merged = false;
        
        for m in 0..merged_count as usize {
            let (mx, my, mcount, mmdx, mmdy) = merged_clusters[m];
            
            let dist_x = (cx - mx).abs();
            let dist_y = (cy - my).abs();
            
            if dist_x < (max_size as i32) && dist_y < (max_size as i32) {
                let total_count = mcount + count;
                merged_clusters[m].0 = (mx * mcount as i32 + cx * count as i32) / total_count as i32;
                merged_clusters[m].1 = (my * mcount as i32 + cy * count as i32) / total_count as i32;
                merged_clusters[m].2 = total_count;
                merged_clusters[m].3 = (mmdx * mcount as i16 + mdx * count as i16) / total_count as i16;
                merged_clusters[m].4 = (mmdy * mcount as i16 + mdy * count as i16) / total_count as i16;
                
                merged = true;
                break;
            }
        }
        
        if !merged && merged_count < MAX_OBSTACLES as u16 {
            merged_clusters[merged_count as usize] = (cx, cy, count, mdx, mdy);
            merged_count += 1;
        }
    }
    
    for m in 0..merged_count as usize {
        let (cx, cy, count, mdx, mdy) = merged_clusters[m];
        
        let size = (count as u16 * 8).clamp(min_size, max_size);
        
        let distance = estimate_distance(size, mdx, mdy);
        let confidence = calculate_confidence(count, mdx, mdy);
        
        result.obstacles[obstacle_count as usize] = Obstacle {
            center_x: cx as i16,
            center_y: cy as i16,
            width: size as i16,
            height: size as i16,
            estimated_distance: distance,
            motion_dx: mdx,
            motion_dy: mdy,
            confidence,
            reserved: 0,
        };
        
        obstacle_count += 1;
    }
    
    result.count = obstacle_count;
    result
}

fn estimate_distance(size: u16, dx: i16, dy: i16) -> i16 {
    let motion_magnitude = (dx.abs() + dy.abs()) as u16;
    
    let mut distance: i16 = 500;
    
    if size > 80 {
        distance = 100;
    } else if size > 60 {
        distance = 200;
    } else if size > 40 {
        distance = 300;
    } else if size > 20 {
        distance = 400;
    }
    
    if motion_magnitude > 10 {
        distance = distance.saturating_sub(50);
    }
    
    distance
}

fn calculate_confidence(count: u16, dx: i16, dy: i16) -> u8 {
    let mut confidence: u8 = 50;
    
    if count >= 10 {
        confidence += 20;
    } else if count >= 5 {
        confidence += 10;
    }
    
    let motion_magnitude = (dx.abs() + dy.abs()) as u16;
    if motion_magnitude > 5 {
        confidence += 15;
    }
    
    confidence.min(100)
}

pub fn find_nearest_obstacle(
    obstacles: &ObstacleDetectionResult,
    robot_x: i16,
    robot_y: i16
) -> Option<(Obstacle, u16)> {
    if obstacles.count == 0 {
        return None;
    }
    
    let mut nearest_idx: usize = 0;
    let mut min_distance: u16 = u16::MAX;
    
    for i in 0..obstacles.count as usize {
        let obstacle = &obstacles.obstacles[i];
        
        let dx = (obstacle.center_x - robot_x) as i32;
        let dy = (obstacle.center_y - robot_y) as i32;
        
        let dist = ((dx * dx + dy * dy) as f32).sqrt() as u16;
        
        if dist < min_distance {
            min_distance = dist;
            nearest_idx = i;
        }
    }
    
    Some((obstacles.obstacles[nearest_idx], min_distance))
}

pub fn is_obstacle_in_path(
    obstacles: &ObstacleDetectionResult,
    direction: MotionDirection,
    safe_distance: u16
) -> bool {
    for i in 0..obstacles.count as usize {
        let obstacle = &obstacles.obstacles[i];
        
        if obstacle.estimated_distance < safe_distance as i16 {
            match direction {
                MotionDirection::Forward => {
                    if obstacle.center_y < IMAGE_HEIGHT as i16 / 2 + 50 {
                        return true;
                    }
                }
                MotionDirection::Backward => {
                    if obstacle.center_y > IMAGE_HEIGHT as i16 / 2 - 50 {
                        return true;
                    }
                }
                MotionDirection::Left => {
                    if obstacle.center_x < IMAGE_WIDTH as i16 / 2 {
                        return true;
                    }
                }
                MotionDirection::Right => {
                    if obstacle.center_x > IMAGE_WIDTH as i16 / 2 {
                        return true;
                    }
                }
                _ => {}
            }
        }
    }
    
    false
}
