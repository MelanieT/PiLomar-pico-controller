# SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
# SPDX-License-Identifier: BSD-3-Clause

set(PROJECT pilomar)
set(PICO_BOARD tdata)

cmake_minimum_required(VERSION 3.12)

set(PICO_SDK_PATH     $ENV{PICO_SDK_PATH})
set(PICO_TINYUSB_PATH ${PICO_SDK_PATH}/lib/tinyusb)
set(LWIP_DIR          ${PICO_SDK_PATH}/lib/lwip)

message("")
message("===== PICO_SDK_PATH     = ${PICO_SDK_PATH}")
message("===== PICO_TINYUSB_PATH = ${PICO_TINYUSB_PATH}")
message("===== LWIP_DIR          = ${LWIP_DIR}")
message("")

include(pico_sdk_import.cmake)
project(${PROJECT} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(webserver)

set (LWIP_INCLUDE_DIRS
    "${LWIP_DIR}/src/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    )
include(${LWIP_DIR}/src/Filelists.cmake)

set(TINYUSB_LIBNETWORKING_SOURCES
    ${PICO_TINYUSB_PATH}/lib/networking/dhserver.c
    ${PICO_TINYUSB_PATH}/lib/networking/rndis_reports.c
    )

add_executable(${PROJECT}
    main.cpp
    Motor.cpp
    usb_descriptors.c
    ${TINYUSB_LIBNETWORKING_SOURCES}
    tusb_lwip_glue.c
    )

target_include_directories(${PROJECT} PUBLIC
    .
    ./include
    ${LWIP_INCLUDE_DIRS}
    ${PICO_TINYUSB_PATH}/src
    ${PICO_TINYUSB_PATH}/lib/networking
    ${PICO_SDK_PATH}/lib/lwip/src/include/lwip/apps/
    )

target_link_libraries(${PROJECT}
    pico_stdlib
    pico_unique_id
    hardware_pwm
    hardware_timer
    hardware_flash
    tinyusb_device
    lwipallapps
    lwipcore
    webserver-lwip
    )

target_compile_options(${PROJECT} PRIVATE
    -Wno-psabi
    )

pico_add_extra_outputs(${PROJECT})

pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 1)

add_custom_command(TARGET pilomar POST_BUILD
    COMMAND /usr/local/bin/openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program build/pilomar.elf verify reset exit"
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMENT Flashing...)
