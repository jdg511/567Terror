# Glitchwave 567 firmware — Illicit Apothecary
# Build (once pico-sdk is installed; see README.md):
#   mkdir build && cd build && cmake .. && make
# Then drag build/glitchwave567.uf2 onto the RPI-RP2 drive.
cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(glitchwave567 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(glitchwave567
    src/main.c
)

pico_generate_pio_header(glitchwave567
    ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio
)

target_include_directories(glitchwave567 PRIVATE src)

target_link_libraries(glitchwave567
    pico_stdlib
    hardware_adc
    hardware_pwm
    hardware_pio
)

# USB console, no UART
pico_enable_stdio_usb(glitchwave567 1)
pico_enable_stdio_uart(glitchwave567 0)

pico_add_extra_outputs(glitchwave567)   # makes the .uf2
