cmake_minimum_required(VERSION 3.12)
set(PICO_BOARD tdata)
#set(PICO_BUILD_BOOT_STAGE2_NAME boot2_is25lp080)
#set(PICO_BUILD_BOOT_STAGE2_NAME boot2_generic_03h)

include(pico_sdk_import.cmake)

project(pilomar C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pilomar
    main.cpp
    Stepper.cpp Stepper.h)

pico_enable_stdio_usb(pilomar 0)
pico_enable_stdio_uart(pilomar 1)
#pico_set_binary_type(pilomar copy_to_ram)

target_link_libraries(pilomar
    pico_stdlib
    pico_multicore
    hardware_pwm
    hardware_timer
)

target_include_directories(pilomar PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(pilomar)

add_custom_command(TARGET pilomar POST_BUILD
    COMMAND openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program build/pilomar.elf verify reset exit"
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMENT Flashing...)