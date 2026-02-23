cmake_minimum_required(VERSION 3.16)

project(stm32_firmware_app C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(fw_app STATIC
    App/src/app_main.c
    App/src/comm_lora.c
    App/src/comm_wifi.c
    App/src/sensor_manager.c
    App/src/signer.c
    App/src/telemetry_packet.c
)

target_include_directories(fw_app
    PUBLIC
        App/include
)

enable_testing()
add_subdirectory(tests)
