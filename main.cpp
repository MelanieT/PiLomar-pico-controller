#include <cstdio>
#include <pico/time.h>
#include <hardware/gpio.h>
#include <vector>
#include "pico/stdio.h"
#include "hardware/pwm.h"
#include "Stepper.h"

#define LED_BLUE  4
#define LED_GREEN 5
#define LED_RED   6
#define STEP_M0   8
#define STEP_M1   9
#define STEP_M2   10
#define RST       19
#define EN0       15
#define STEP0     16
#define DIR0      17
#define FAULT0    18
#define EN1       11
#define STEP1     12
#define DIR1      13
#define FAULT1    14

#define SPARE_A1  22
#define SPARE_A2  23
#define SPARE_A3  24

#define SPARE_B1  26
#define SPARE_B2  27
#define SPARE_B3  28

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
int main()
{
    stdio_init_all();

    std::vector<uint> gpios_out = {4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    std::vector<uint> gpios_in = {};

    for (auto p: gpios_out)
    {
        gpio_init(p);
        gpio_set_dir(p, true);
        gpio_put(p, true);
    }

    for (auto p: gpios_in)
    {
        gpio_init(p);
        gpio_set_dir(p, false);
    }

    gpio_put(LED_GREEN, false);

    // Reset stepper drivers
    gpio_put(RST, false);
    sleep_ms(10);
    gpio_put(RST, true);

    // Set step ouputs to low
    gpio_put(STEP0, false);
    gpio_put(STEP1, false);

    // Set to full steps
    gpio_put(STEP_M0, true);
    gpio_put(STEP_M1, true);
    gpio_put(STEP_M2, true);

    gpio_put(DIR0, true); // false is up
    gpio_put(DIR1, true); // false is clockwise

//    gpio_set_function(STEP0, GPIO_FUNC_PWM);
//    uint step0slice = pwm_gpio_to_slice_num(STEP0);
//    pwm_set_clkdiv_int_frac(step0slice, 250, 0);
//    pwm_set_wrap(step0slice, 500/16);
//    pwm_set_chan_level(step0slice, 0, 250/16);
//    pwm_set_enabled(step0slice, true);
//
//    gpio_set_function(STEP1, GPIO_FUNC_PWM);
//    uint step1slice = pwm_gpio_to_slice_num(STEP1);
//    pwm_set_clkdiv_int_frac(step1slice, 250, 0);
//    pwm_set_wrap(step1slice, 50000);
//    pwm_set_chan_level(step1slice, 0, 25000);
//    pwm_set_enabled(step1slice, true);

    Stepper stepper((Pins){STEP1, DIR1, EN1, -1}, 500);
    stepper.addSchedule(0, 0, 0);

    while (true)
    {
        sleep_ms(500);
        gpio_put(LED_RED, false);
//        gpio_put(STEP0, false);
        sleep_ms(500);
        gpio_put(LED_RED, true);
//        gpio_put(STEP0, true);
        //printf("Hello, world!\r\n");
    }
}
#pragma clang diagnostic pop
