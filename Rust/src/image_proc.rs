use crate::shared_types::*;

pub fn grayscale_threshold(image: &[u8], threshold: u8) -> [u8; IMAGE_SIZE] {
    let mut output = [0u8; IMAGE_SIZE];
    
    for i in 0..IMAGE_SIZE {
        if image[i] > threshold {
            output[i] = 255;
        } else {
            output[i] = 0;
        }
    }
    
    output
}

pub fn gaussian_blur_3x3(image: &[u8]) -> [u8; IMAGE_SIZE] {
    let mut output = [0u8; IMAGE_SIZE];
    let kernel = [
        1, 2, 1,
        2, 4, 2,
        1, 2, 1
    ];
    let kernel_sum: i32 = 16;
    
    for y in 1..(IMAGE_HEIGHT - 1) {
        for x in 1..(IMAGE_WIDTH - 1) {
            let idx = (y as usize) * (IMAGE_WIDTH as usize) + (x as usize);
            let mut sum: i32 = 0;
            
            for ky in 0..3 {
                for kx in 0..3 {
                    let px = x as i32 - 1 + kx as i32;
                    let py = y as i32 - 1 + ky as i32;
                    let pidx = (py as usize) * (IMAGE_WIDTH as usize) + (px as usize);
                    sum += (image[pidx] as i32) * (kernel[ky * 3 + kx] as i32);
                }
            }
            
            output[idx] = ((sum / kernel_sum) as u8);
        }
    }
    
    output
}

pub fn sobel_edge_detection(image: &[u8]) -> [u8; IMAGE_SIZE] {
    let mut output = [0u8; IMAGE_SIZE];
    
    let gx = [
        -1, 0, 1,
        -2, 0, 2,
        -1, 0, 1
    ];
    
    let gy = [
        -1, -2, -1,
         0,  0,  0,
         1,  2,  1
    ];
    
    for y in 1..(IMAGE_HEIGHT - 1) {
        for x in 1..(IMAGE_WIDTH - 1) {
            let idx = (y as usize) * (IMAGE_WIDTH as usize) + (x as usize);
            let mut sum_x: i32 = 0;
            let mut sum_y: i32 = 0;
            
            for ky in 0..3 {
                for kx in 0..3 {
                    let px = x as i32 - 1 + kx as i32;
                    let py = y as i32 - 1 + ky as i32;
                    let pidx = (py as usize) * (IMAGE_WIDTH as usize) + (px as usize);
                    let pixel = image[pidx] as i32;
                    
                    sum_x += pixel * (gx[ky * 3 + kx] as i32);
                    sum_y += pixel * (gy[ky * 3 + kx] as i32);
                }
            }
            
            let magnitude = ((sum_x.abs() + sum_y.abs()) as u32).min(255) as u8;
            output[idx] = magnitude;
        }
    }
    
    output
}

pub fn image_difference(img1: &[u8], img2: &[u8]) -> [u8; IMAGE_SIZE] {
    let mut diff = [0u8; IMAGE_SIZE];
    
    for i in 0..IMAGE_SIZE {
        let a = img1[i] as i16;
        let b = img2[i] as i16;
        diff[i] = ((a - b).abs() as u8);
    }
    
    diff
}

pub fn calculate_histogram(image: &[u8]) -> [u32; 256] {
    let mut hist = [0u32; 256];
    
    for &pixel in image {
        hist[pixel as usize] += 1;
    }
    
    hist
}

pub fn otsu_threshold(image: &[u8]) -> u8 {
    let hist = calculate_histogram(image);
    let total = IMAGE_SIZE as u32;
    
    let mut sum: f32 = 0.0;
    for i in 0..256 {
        sum += (i as f32) * (hist[i] as f32);
    }
    
    let mut sum_b: f32 = 0.0;
    let mut w_b: u32 = 0;
    let mut w_f: u32;
    
    let mut var_max: f32 = 0.0;
    let mut threshold: u8 = 0;
    
    for i in 0..256 {
        w_b += hist[i];
        if w_b == 0 {
            continue;
        }
        
        w_f = total - w_b;
        if w_f == 0 {
            break;
        }
        
        sum_b += (i as f32) * (hist[i] as f32);
        
        let m_b = sum_b / (w_b as f32);
        let m_f = (sum - sum_b) / (w_f as f32);
        
        let var_between = (w_b as f32) * (w_f as f32) * (m_b - m_f) * (m_b - m_f);
        
        if var_between > var_max {
            var_max = var_between;
            threshold = i as u8;
        }
    }
    
    threshold
}

pub fn integral_image(image: &[u8]) -> [u32; IMAGE_SIZE] {
    let mut integral = [0u32; IMAGE_SIZE];
    
    for y in 0..IMAGE_HEIGHT {
        let mut row_sum: u32 = 0;
        for x in 0..IMAGE_WIDTH {
            let idx = (y as usize) * (IMAGE_WIDTH as usize) + (x as usize);
            row_sum += image[idx] as u32;
            
            if y == 0 {
                integral[idx] = row_sum;
            } else {
                let above_idx = ((y - 1) as usize) * (IMAGE_WIDTH as usize) + (x as usize);
                integral[idx] = integral[above_idx] + row_sum;
            }
        }
    }
    
    integral
}

pub fn box_filter(integral: &[u32], x: u16, y: u16, width: u16, height: u16) -> u32 {
    if x >= IMAGE_WIDTH || y >= IMAGE_HEIGHT {
        return 0;
    }
    
    let x1 = x.saturating_sub(1);
    let y1 = y.saturating_sub(1);
    let x2 = (x + width).min(IMAGE_WIDTH - 1);
    let y2 = (y + height).min(IMAGE_HEIGHT - 1);
    
    let idx11 = (y1 as usize) * (IMAGE_WIDTH as usize) + (x1 as usize);
    let idx12 = (y1 as usize) * (IMAGE_WIDTH as usize) + (x2 as usize);
    let idx21 = (y2 as usize) * (IMAGE_WIDTH as usize) + (x1 as usize);
    let idx22 = (y2 as usize) * (IMAGE_WIDTH as usize) + (x2 as usize);
    
    let a = if x1 > 0 && y1 > 0 { integral[idx11] } else { 0 };
    let b = if y1 > 0 { integral[idx12] } else { 0 };
    let c = if x1 > 0 { integral[idx21] } else { 0 };
    let d = integral[idx22];
    
    d + a - b - c
}
