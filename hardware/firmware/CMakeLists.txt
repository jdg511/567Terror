# ============================================================================
#  Glitchwave 567 — pedal firmware (Raspberry Pi Pico / RP2040, module U20)
#  Illicit Apothecary
#
#  Build (Linux/macOS):
#      export PICO_SDK_PATH=/path/to/pico-sdk
#      cmake -B build -G Ninja
#      cmake --build build
#  Build (Windows, from a Developer Command Prompt or with the Pico extension):
#      cmake -B build -G "Ninja"
#      cmake --build build
#
#  The result is build/glitchwave567.uf2 — hold BOOTSEL while plugging the
#  pedal into USB (or type `bootsel` on the console) and drag it across.
# ============================================================================
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico CACHE STRING "Target board")

include(pico_sdk_import.cmake)

project(glitchwave567 C CXX ASM)
pico_sdk_init()

# Bump this when you change behaviour, so `status` tells the truth.
set(GW_FW_VERSION "fw-1.0")

add_executable(glitchwave567
    src/main.c
    src/tunables.c
    src/hw_io.c
    src/mod_system.c
    src/stomps.c
    src/control.c
    src/console.c
    src/selftest.c
    src/flash_store.c
)

pico_generate_pio_header(glitchwave567
    ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio
)

target_include_directories(glitchwave567 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_compile_definitions(glitchwave567 PRIVATE
    GW_FW_VERSION=\"${GW_FW_VERSION}\"
    # flash_safe_execute() needs to be able to park the other core.
    # (Note the _PICO_ in the middle -- the obvious spelling is a no-op.)
    PICO_FLASH_SAFE_EXECUTE_PICO_SUPPORT_MULTICORE_LOCKOUT=1
)

target_compile_options(glitchwave567 PRIVATE
    -Wall -Wextra
    -Wno-unused-parameter
    # DO NOT add -ffast-math here. It implies -ffinite-math-only, which lets the
    # compiler assume NaN cannot happen -- it then DELETES the NaN guards in
    # hw_io.c and mod_system.c, and inverts gw_clampf so that clamping a NaN
    # returns the UPPER bound. A NaN anywhere in the law chain would come out as
    # a CV at 100 %: full volume, gate forced open. Measured in the disassembly,
    # not theoretical.
    -fno-finite-math-only
)

target_link_libraries(glitchwave567
    pico_stdlib
    pico_multicore
    pico_flash
    hardware_pwm
    hardware_adc
    hardware_pio
    hardware_flash
    hardware_sync
    hardware_watchdog
    hardware_clocks
)

# USB serial for the console. UART off so GP0/GP1 stay free as CV outputs —
# they are PWM_FREQ and PWM_DIRT on this board, so this is not optional.
pico_enable_stdio_usb(glitchwave567 1)
pico_enable_stdio_uart(glitchwave567 0)

pico_add_extra_outputs(glitchwave567)
