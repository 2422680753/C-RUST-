cmake_minimum_required(VERSION 3.16)

project(C_RUST_Visual_Odometry
    VERSION 0.1.0
    DESCRIPTION "STM32H7 Visual Odometry with C++ and Rust"
    LANGUAGES C CXX ASM
)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 交叉编译配置
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 编译器设置
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# 目标 MCU 设置
set(MCU_FAMILY STM32H7)
set(MCU_LINE STM32H743xx)
set(MCU_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32H743VITx_FLASH.ld)

# 编译标志
set(MCU_FLAGS "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS "${MCU_FLAGS} -Wall -fdata-sections -ffunction-sections")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

set(CMAKE_CXX_FLAGS "${MCU_FLAGS} -Wall -fdata-sections -ffunction-sections -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

set(CMAKE_ASM_FLAGS "${MCU_FLAGS} -x assembler-with-cpp")

# 链接标志
set(CMAKE_EXE_LINKER_FLAGS "${MCU_FLAGS} -T${MCU_LINKER_SCRIPT} -Wl,--gc-sections -specs=nano.specs -specs=nosys.specs")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-g")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "")

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/motor
    ${CMAKE_CURRENT_SOURCE_DIR}/comm
    ${CMAKE_CURRENT_SOURCE_DIR}/debug
)

# 源文件
set(SOURCES
    main.cpp
    drivers/gpio.cpp
    drivers/uart.cpp
    drivers/timer.cpp
    motor/motor_control.cpp
    comm/comm_manager.cpp
    debug/uart_debug.cpp
)

# Rust 静态库路径
set(RUST_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Rust/target/thumbv7em-none-eabihf/release)
set(RUST_LIB ${RUST_LIB_DIR}/libimage_processing_core.a)

# 可执行文件
add_executable(${PROJECT_NAME}.elf ${SOURCES})

# 链接 Rust 库
target_link_libraries(${PROJECT_NAME}.elf PRIVATE
    ${RUST_LIB}
    -Wl,--whole-archive
    -Wl,--no-whole-archive
)

# 生成二进制文件
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} ${PROJECT_NAME}.elf
    COMMENT "Building ${PROJECT_NAME}.bin"
)

# 生成 Intel HEX 文件
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMENT "Building ${PROJECT_NAME}.hex"
)

# 构建 Rust 库的依赖
add_custom_target(rust_lib ALL
    COMMAND cargo build --release --target thumbv7em-none-eabihf
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../Rust
    COMMENT "Building Rust library"
)

add_dependencies(${PROJECT_NAME}.elf rust_lib)

# 安装规则
install(TARGETS ${PROJECT_NAME}.elf
    RUNTIME DESTINATION bin
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
    DESTINATION bin
)

# 自定义目标：清理
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMAND cargo clean --target thumbv7em-none-eabihf
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../Rust
    COMMENT "Cleaning all build artifacts"
)

# 自定义目标：烧录（需要 OpenOCD 或其他烧录工具）
add_custom_target(flash
    COMMAND openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "program ${PROJECT_NAME}.elf verify reset exit"
    DEPENDS ${PROJECT_NAME}.elf
    COMMENT "Flashing ${PROJECT_NAME}.elf to device"
)

# 自定义目标：调试
add_custom_target(debug
    COMMAND arm-none-eabi-gdb ${PROJECT_NAME}.elf -ex "target remote :3333"
    DEPENDS ${PROJECT_NAME}.elf
    COMMENT "Starting GDB debug session"
)
