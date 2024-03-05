// SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
// SPDX-License-Identifier: BSD-3-Clause

#include <cstdio>
#include <pico/time.h>
#include <hardware/gpio.h>
#include <vector>
#include <pico/stdlib.h>
#include <hardware/flash.h>
#include "pico/stdio.h"
#include "Motor.h"
#include "tusb.h"
#include "tusb_lwip_glue.h"
#include "webserver/WebServer-lwip.h"
#include "webserver/ApiServer.h"
#include "UrlMapper.h"
#include "nlohmann/json.hpp"

#define MODE_DOOR 0
#define MODE_CAMERA 1
#define MODE MODE_DOOR

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
#define VCC0      18
#define EN1       11
#define STEP1     12
#define DIR1      13
#define VCC1      14

#define SPARE_A1  22
#define SPARE_A2  23
#define SPARE_A3  24

#define SPARE_B1  26
#define SPARE_B2  27
#define SPARE_B3  28

#define AZIMUTH_MAX_SPEED 8000
#define AZIMUTH_STEPS_PER_REVOLUTION 384000
#define AZIMUTH_MAX_STEPS 384000 //373334

#define ELEVATION_MAX_SPEED 8000
#define ELEVATION_STEPS_PER_REVOLUTION 384000
#define ELEVATION_MAX_STEPS 120000
#define ELEVATION_ZERO_POINT 22500

#define DOOR_MAX_STEPS 2125
#define DOOR_MAX_SPEED 400

using namespace nlohmann;

uint8_t macaddr[6];

uint8_t tud_network_mac_address[6];

Motor *stepper0;
Motor *stepper1;

WebServerLwip webserver;

class PilomarApi
{
public:
    static void Init()
    {
        UrlMapper::AddMapping("GET", "/info", &info);
        UrlMapper::AddMapping("POST", "/move", &move);
    }
private:
#if MODE == MODE_DOOR
    static void move(const HttpRequest& request, HttpResponse& response)
    {
        auto payload = json::parse(request.body(), nullptr, false);
        if (payload.is_discarded())
        {
            response.setStatusCode(HttpStatus::Code::BadRequest);
            return;
        }

        if (stepper0->isRunning() || stepper1->isRunning())
        {
            response.setStatusCode(HttpStatus::Code::PreconditionFailed);
            return;
        }

        auto mode = payload.value<std::string>("mode", "move");

        if (mode == "move")
        {
            int position = payload.value("position", -1);
            int speed = payload.value("speed", DOOR_MAX_SPEED);

            if (position < 0 || position > DOOR_MAX_STEPS || speed < 10 || speed > DOOR_MAX_SPEED)
            {
                response.setStatusCode(HttpStatus::Code::BadRequest);
                return;
            }
            stepper0->runToTarget(speed, position);
            stepper1->runToTarget(speed, position);
        }
        else if (mode == "learn")
        {
            stepper0->setCurrentPosition(DOOR_MAX_STEPS);
            stepper1->setCurrentPosition(DOOR_MAX_STEPS);
            stepper0->runToTarget(DOOR_MAX_SPEED, 0);
            stepper1->runToTarget(DOOR_MAX_SPEED, 0);
        }
        else
        {
            response.setStatusCode(HttpStatus::Code::BadRequest);
            return;
        }

        response.setBody(
                json{
                    {"result", "ok"}
                }.dump()
            );
    }
#elif MODE == MODE_CAMERA
#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-branch-clone"
    static void move(const HttpRequest& request, HttpResponse& response)
    {
        auto payload = json::parse(request.body(), nullptr, false);
        if (payload.is_discarded())
        {
            response.setStatusCode(HttpStatus::Code::BadRequest);
            return;
        }

        auto motor = payload.value<std::string>("motor", "");
        Motor *m = nullptr;
        if (motor == "azimuth")
            m = stepper1;
        else if (motor == "elevation")
            m = stepper0;

        if (!m)
        {
            response.setStatusCode(HttpStatus::Code::BadRequest);
            return;
        }

//        if (m->isRunning())
//        {
//            response.setStatusCode(HttpStatus::Code::PreconditionFailed);
//            return;
//        }

        int maxSpeed = motor == "elevation" ? ELEVATION_MAX_SPEED : AZIMUTH_MAX_SPEED;
        int maxSteps = motor == "elevation" ? ELEVATION_MAX_STEPS : AZIMUTH_MAX_STEPS;

        int position = payload.value("position", -1);
        auto speed = payload.value<double>("speed", (double)maxSpeed);

        if (position < 0 || position > maxSteps || speed < 0.0001 || speed > maxSpeed)
        {
            response.setStatusCode(HttpStatus::Code::BadRequest);
            return;
        }

        m->runToTarget(speed, position);

        response.setBody(
            json{
                {"result", "ok"}
            }.dump()
        );
    }
#pragma clang diagnostic pop
#endif
    static void info(const HttpRequest& request, HttpResponse& response)
    {
        response.setBody(
            json{
#if MODE == MODE_DOOR
                {"type", "door"},
                {"max_steps", DOOR_MAX_STEPS},
                {"max_speed", DOOR_MAX_SPEED},
                {"position_left", stepper0->getCurrentPosition()},
                {"position_right", stepper1->getCurrentPosition()},
                {"closed_position", 0},
                {"open_position", DOOR_MAX_STEPS}
#elif MODE == MODE_CAMERA
                {"type", "camera"},
#endif
            }.dump()
        );
    }
};

void usbd_serial_init(void)
{
    uint8_t id[8];

    flash_get_unique_id(id);

    tud_network_mac_address[0] = 0xb8;
    tud_network_mac_address[1] = 0x27;
    tud_network_mac_address[2] = 0xeb;
    tud_network_mac_address[3] = id[5];
    tud_network_mac_address[4] = id[6];
    tud_network_mac_address[5] = id[7];
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
int main()
{
    usbd_serial_init();
    tusb_init();
    stdio_init_all();

    init_lwip();

    wait_for_netif_is_up();

    dhcpd_init();

    webserver.Init();
    PilomarApi::Init();

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

#if MODE == MODE_CAMERA
    gpio_put(STEP_M0, true);
    gpio_put(STEP_M1, true);
    gpio_put(STEP_M2, false);
#elif MODE == MODE_DOOR
    gpio_put(STEP_M0, false);
    gpio_put(STEP_M1, false);
    gpio_put(STEP_M2, false);
#endif


    gpio_put(DIR0, true); // false is up
    gpio_put(DIR1, true); // false is clockwise

//    Motor stepper((Pins){STEP0, DIR0, EN0, SPARE_B3, false, true}, ELEVATION_STEPS_PER_REVOLUTION, ELEVATION_MAX_STEPS);
//    Motor stepper0((Pins){STEP0, DIR0, EN0, -1, false, false}, AZIMUTH_STEPS_PER_REVOLUTION, AZIMUTH_MAX_STEPS);
//    Motor stepper1((Pins){STEP1, DIR1, EN1, -1, false, false}, AZIMUTH_STEPS_PER_REVOLUTION, AZIMUTH_MAX_STEPS);
//

#if MODE == MODE_DOOR
    stepper0 = new Motor((Pins){STEP0, DIR0, EN0}, (Options){.autoPowerOff = true, .reverse = true, .microsteps = 1}, DOOR_MAX_STEPS, DOOR_MAX_STEPS);
    stepper1 = new Motor((Pins){STEP1, DIR1, EN1}, (Options){.autoPowerOff = true, .reverse = true, .microsteps = 1}, DOOR_MAX_STEPS, DOOR_MAX_STEPS);
#elif MODE == MODE_CAMERA
    stepper0 = new Motor((Pins){STEP0, DIR0, EN0}, (Options){.reverse = true, .endstop = SPARE_B3, .dirToEndstop = false}, ELEVATION_STEPS_PER_REVOLUTION, ELEVATION_MAX_STEPS);
    stepper1 = new Motor((Pins){STEP1, DIR1, EN1}, (Options){}, AZIMUTH_STEPS_PER_REVOLUTION, AZIMUTH_MAX_STEPS);
#endif

    uint64_t ticks = 0;

    while (true)
    {
//        if (tud_cdc_n_available(1))
//        {
//            char c = tud_cdc_n_read_char(1);
//            tud_cdc_n_write_char(1, c);
//            tud_cdc_n_write_flush(1);
//        }
        ++ticks;

        if (!(ticks % 5000))
        {
//            printf("Blink\n");
            gpio_put(LED_RED, !gpio_get_out_level(LED_RED));
        }

        tud_task();
        service_traffic();
        webserver.ProcessMessages(ApiServer::RequestHandler);

        sleep_us(100);
    }
}
#pragma clang diagnostic pop
