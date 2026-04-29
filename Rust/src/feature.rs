use crate::shared_types::*;

const FAST_CIRCLE_OFFSETS: [(i32, i32); 16] = [
    (0, -3), (1, -3), (2, -2), (3, -1),
    (3, 0), (3, 1), (2, 2), (1, 3),
    (0, 3), (-1, 3), (-2, 2), (-3, 1),
    (-3, 0), (-3, -1), (-2, -2), (-1, -3)
];

pub fn fast_detect(
    image: &[u8],
    threshold: u8,
    max_corners: u16,
    min_distance: u8
) -> CornerDetectionResult {
    let mut result = CornerDetectionResult::new();
    let mut corner_count: u16 = 0;
    
    const BORDER: i32 = 3;
    const MIN_CONSECUTIVE: usize = 9;
    
    for y in BORDER..(IMAGE_HEIGHT as i32 - BORDER) {
        for x in BORDER..(IMAGE_WIDTH as i32 - BORDER) {
            if corner_count >= max_corners {
                break;
            }
            
            let center_idx = (y as usize) * (IMAGE_WIDTH as usize) + (x as usize);
            let center_pixel = image[center_idx];
            
            let threshold_high = center_pixel.saturating_add(threshold);
            let threshold_low = center_pixel.saturating_sub(threshold);
            
            let mut bright_count: usize = 0;
            let mut dark_count: usize = 0;
            
            let mut is_corner = false;
            
            'pixel_check: for offset in 0..16 {
                let (dx, dy) = FAST_CIRCLE_OFFSETS[offset];
                let px = x + dx;
                let py = y + dy;
                let pidx = (py as usize) * (IMAGE_WIDTH as usize) + (px as usize);
                let pixel = image[pidx];
                
                if pixel > threshold_high {
                    bright_count += 1;
                    dark_count = 0;
                } else if pixel < threshold_low {
                    dark_count += 1;
                    bright_count = 0;
                } else {
                    bright_count = 0;
                    dark_count = 0;
                }
                
                if bright_count >= MIN_CONSECUTIVE || dark_count >= MIN_CONSECUTIVE {
                    is_corner = true;
                    break 'pixel_check;
                }
            }
            
            if is_corner {
                let response = calculate_fast_response(image, x, y, center_pixel, threshold);
                
                if corner_count == 0 {
                    result.corners[0] = Corner {
                        x: x as i16,
                        y: y as i16,
                        response,
                        reserved: 0,
                    };
                    corner_count = 1;
                } else {
                    let mut too_close = false;
                    
                    for i in 0..corner_count as usize {
                        let existing = &result.corners[i];
                        let dx = (x - existing.x as i32).abs();
                        let dy = (y - existing.y as i32).abs();
                        
                        if dx < min_distance as i32 && dy < min_distance as i32 {
                            if response > existing.response {
                                result.corners[i] = Corner {
                                    x: x as i16,
                                    y: y as i16,
                                    response,
                                    reserved: 0,
                                };
                            }
                            too_close = true;
                            break;
                        }
                    }
                    
                    if !too_close && corner_count < max_corners {
                        result.corners[corner_count as usize] = Corner {
                            x: x as i16,
                            y: y as i16,
                            response,
                            reserved: 0,
                        };
                        corner_count += 1;
                    }
                }
            }
        }
    }
    
    result.count = corner_count;
    result
}

fn calculate_fast_response(
    image: &[u8],
    x: i32,
    y: i32,
    center: u8,
    threshold: u8
) -> u8 {
    let mut max_diff: u8 = 0;
    
    for offset in 0..16 {
        let (dx, dy) = FAST_CIRCLE_OFFSETS[offset];
        let px = x + dx;
        let py = y + dy;
        let pidx = (py as usize) * (IMAGE_WIDTH as usize) + (px as usize);
        let pixel = image[pidx];
        
        let diff = if pixel > center {
            pixel - center
        } else {
            center - pixel
        };
        
        if diff > max_diff {
            max_diff = diff;
        }
    }
    
    max_diff.saturating_sub(threshold)
}

const LUCAS_KANADE_WINDOW: i32 = 7;
const LUCAS_KANADE_HALF_WINDOW: i32 = 3;
const MAX_ITERATIONS: u8 = 10;
const EPSILON: f32 = 0.01;

pub fn optical_flow_tracking(
    prev_image: &[u8],
    curr_image: &[u8],
    prev_corners: &CornerDetectionResult,
    max_corners: u16
) -> OpticalFlowResult {
    let mut result = OpticalFlowResult::new();
    let mut tracked_count: u16 = 0;
    
    let corner_count = prev_corners.count.min(max_corners);
    
    for i in 0..corner_count as usize {
        let corner = &prev_corners.corners[i];
        let x = corner.x as i32;
        let y = corner.y as i32;
        
        if x < LUCAS_KANADE_HALF_WINDOW || 
           x >= IMAGE_WIDTH as i32 - LUCAS_KANADE_HALF_WINDOW ||
           y < LUCAS_KANADE_HALF_WINDOW || 
           y >= IMAGE_HEIGHT as i32 - LUCAS_KANADE_HALF_WINDOW {
            continue;
        }
        
        let (dx, dy, success) = lucas_kanade_tracker(
            prev_image,
            curr_image,
            x,
            y
        );
        
        if success {
            result.prev_corners[tracked_count as usize] = *corner;
            result.curr_corners[tracked_count as usize] = Corner {
                x: (x + dx) as i16,
                y: (y + dy) as i16,
                response: corner.response,
                reserved: 0,
            };
            result.dx[tracked_count as usize] = dx as i16;
            result.dy[tracked_count as usize] = dy as i16;
            tracked_count += 1;
        }
    }
    
    result.tracked_count = tracked_count;
    result
}

fn lucas_kanade_tracker(
    prev_image: &[u8],
    curr_image: &[u8],
    x: i32,
    y: i32
) -> (i32, i32, bool) {
    let mut u = 0.0;
    let mut v = 0.0;
    
    let window_size = LUCAS_KANADE_WINDOW * LUCAS_KANADE_WINDOW;
    
    for _ in 0..MAX_ITERATIONS {
        let mut ixx = 0.0;
        let mut ixy = 0.0;
        let mut iyy = 0.0;
        let mut ixt = 0.0;
        let mut iyt = 0.0;
        
        for wy in -LUCAS_KANADE_HALF_WINDOW..=LUCAS_KANADE_HALF_WINDOW {
            for wx in -LUCAS_KANADE_HALF_WINDOW..=LUCAS_KANADE_HALF_WINDOW {
                let px = x + wx;
                let py = y + wy;
                
                if px < 1 || px >= IMAGE_WIDTH as i32 - 1 ||
                   py < 1 || py >= IMAGE_HEIGHT as i32 - 1 {
                    continue;
                }
                
                let pidx = (py as usize) * (IMAGE_WIDTH as usize) + (px as usize);
                
                let i_prev = prev_image[pidx] as f32;
                
                let cx = (x as f32) + u + (wx as f32);
                let cy = (y as f32) + v + (wy as f32);
                
                let i_curr = bilinear_interpolate(curr_image, cx, cy);
                
                let ix = (get_pixel(prev_image, px + 1, py) - 
                          get_pixel(prev_image, px - 1, py)) as f32 * 0.5;
                let iy = (get_pixel(prev_image, px, py + 1) - 
                          get_pixel(prev_image, px, py - 1)) as f32 * 0.5;
                let it = i_curr - i_prev;
                
                ixx += ix * ix;
                ixy += ix * iy;
                iyy += iy * iy;
                ixt += ix * it;
                iyt += iy * it;
            }
        }
        
        let det = ixx * iyy - ixy * ixy;
        
        if det.abs() < 1e-6 {
            return (0, 0, false);
        }
        
        let inv_det = 1.0 / det;
        
        let du = (-iyy * ixt + ixy * iyt) * inv_det;
        let dv = (ixy * ixt - ixx * iyt) * inv_det;
        
        u += du;
        v += dv;
        
        if du.abs() < EPSILON && dv.abs() < EPSILON {
            break;
        }
    }
    
    if u.abs() > 20.0 || v.abs() > 20.0 {
        return (0, 0, false);
    }
    
    (u.round() as i32, v.round() as i32, true)
}

fn get_pixel(image: &[u8], x: i32, y: i32) -> u8 {
    if x < 0 || x >= IMAGE_WIDTH as i32 || y < 0 || y >= IMAGE_HEIGHT as i32 {
        return 0;
    }
    let idx = (y as usize) * (IMAGE_WIDTH as usize) + (x as usize);
    image[idx]
}

fn bilinear_interpolate(image: &[u8], x: f32, y: f32) -> f32 {
    let x0 = x.floor() as i32;
    let y0 = y.floor() as i32;
    let x1 = x0 + 1;
    let y1 = y0 + 1;
    
    let fx = x - x0 as f32;
    let fy = y - y0 as f32;
    
    let v00 = get_pixel(image, x0, y0) as f32;
    let v01 = get_pixel(image, x0, y1) as f32;
    let v10 = get_pixel(image, x1, y0) as f32;
    let v11 = get_pixel(image, x1, y1) as f32;
    
    let v0 = v00 * (1.0 - fx) + v10 * fx;
    let v1 = v01 * (1.0 - fx) + v11 * fx;
    let v = v0 * (1.0 - fy) + v1 * fy;
    
    v
}
