//
// Created by Melanie on 04/02/2024.
//

#include <algorithm>
#include <hardware/gpio.h>
#include "Stepper.h"

#include "hardware/pwm.h"
#include "hardware/irq.h"

#define RAMPING_THRESHOLD 200

// This is the smallest wrap that doesn't stall the motor, it represents 1000 full steps per second
// 31.25 * 16 == 500, 1000 * 16 = 16000 microsteps
#define MIN_WRAP 31.25

// This is the highest wrap resulting in an integral frequency, it represents 1/2 full step per second
#define MAX_WRAP 62500.0

#define MIN_HZ 8
#define MAX_HZ 16000

#define MAX_DIRECT_START 15960

bool Stepper::initialized = false;
std::list<Stepper *> Stepper::instances;

Stepper::Stepper(Pins pins, int microStepsPerDegree)
{
    this->pins = pins;
    this->microStepsPerDegree = microStepsPerDegree;

    printf("Pins: step %d dir %d enable %d\r\n", this->pins.step, this->pins.dir, this->pins.enable);

    sliceNumber = pwm_gpio_to_slice_num(pins.step);

    if (std::find_if(Stepper::instances.begin(), Stepper::instances.end(),
                     [this](const Stepper *stepper) {return stepper->sliceNumber == this->sliceNumber;}) != Stepper::instances.end())
    {
        printf("Duplicate slices are not supported\r\n");
        return; // Should throw here but that is disabled on Pico
    }

    Stepper::instances.emplace_back(this);

    if (!initialized)
    {
        irq_set_exclusive_handler(PWM_IRQ_WRAP, &Stepper::interruptHandler);
        irq_set_enabled(PWM_IRQ_WRAP, true);

        initialized = true;
    }
    pwm_set_irq_enabled(sliceNumber, true);

    pwm_set_clkdiv_int_frac(sliceNumber, 250, 0);
}

Stepper::~Stepper()
{
    Stepper::instances.remove(this);
}

int Stepper::hzToWrap(int hz)
{
    return (int)(MAX_WRAP + ((MIN_WRAP - MAX_WRAP) / (MAX_HZ - MIN_HZ)) * (hz - MIN_HZ));
}

void Stepper::interruptHandler()
{
    for (int slice = 0; slice < 8; slice++)
    {
        if (pwm_get_irq_status_mask() & (1 << slice))
        {
            auto it = std::find_if(Stepper::instances.begin(), Stepper::instances.end(),
                                   [slice](Stepper *stepper) {return stepper->sliceNumber == slice;});
            if (it != Stepper::instances.end())
            {
                (*it)->handleSpecificInterrupt();
            }
            pwm_clear_irq(slice);
        }
    }
}

void Stepper::handleSpecificInterrupt()
{
    // PWM interrupt handler for this slice
    if (!microStepsToGo)
    {
        disableStepper();
    }

    microStepsToGo--;

    if (microStepsToGo < rampSteps && !rampingDown)
    {
        printf("Ramp down\r\n");
        rampingDown = true;
        rampingUp = false;
        rampCounter = 1000;
    }

    if (rampingDown)
    {
        if (!--rampCounter)
        {
            currentHz -= 16;

            if (currentHz < MAX_DIRECT_START)
            {
                currentHz = MAX_DIRECT_START;
                rampingDown = false;
            }

            int wrap = hzToWrap(currentHz);
            printf("Setting pwm, hz=%d, wrap %d\r\n", currentHz, wrap);

            pwm_set_wrap(sliceNumber, hzToWrap(currentHz));
            pwm_set_chan_level(sliceNumber, 0, hzToWrap(currentHz) / 2);
            pwm_set_enabled(sliceNumber, true);

            rampCounter = 1000;
        }
    }

    if (rampingUp)
    {
        if (!--rampCounter)
        {
            currentHz += 16;

            if (currentHz >= targetHz)
            {
                currentHz = targetHz;
                rampingUp = false;
            }

            int wrap = hzToWrap(currentHz);
            printf("Setting pwm, hz=%d, wrap %d\r\n", currentHz, wrap);

            pwm_set_wrap(sliceNumber, hzToWrap(currentHz));
            pwm_set_chan_level(sliceNumber, 0, hzToWrap(currentHz) / 2);
            pwm_set_enabled(sliceNumber, true);

            rampCounter = 1000;
        }
    }
}

void Stepper::setRate(float hz)
{
    if ( hz > MAX_HZ)
        hz = MAX_HZ;

    if (hz < MIN_HZ)
    {
        printf("Setting slow rate\r\n");

        int period = (int)(1000000.0 / (hz * 2.0));

        gpio_set_function(pins.step, GPIO_FUNC_SIO);
//        gpio_init(pins.step);
        gpio_set_dir(pins.step, true);
        gpio_put(pins.step, false);
        gpio_put(pins.enable, false);

        add_repeating_timer_us(period, alarmHandler, this, &timerData);
        running = true;

        printf("Added timer, period %d\r\n", period);

        return;
    }

    if (hz >= MAX_DIRECT_START)
    {
        targetHz = (int)hz;
        hz = MAX_DIRECT_START;
        currentHz = MAX_DIRECT_START;

        printf("Target Hz %d hz %f\r\n", targetHz, hz);
        rampSteps = (targetHz - (int)hz) / 16 * 1000 + 1;
        rampCounter = 1000;
        rampingUp = true;

        printf("Ramp steps %d\r\n", rampSteps);
    }

    printf("Setting rate, hz %d, wrap %d\r\n", (int)hz, hzToWrap((int)hz));

    gpio_set_function(pins.step, GPIO_FUNC_PWM);
    gpio_put(pins.enable, false);
    pwm_set_wrap(sliceNumber, hzToWrap((int)hz));
    pwm_set_chan_level(sliceNumber, 0, hzToWrap((int)hz) / 2);
    pwm_set_enabled(sliceNumber, true);
}

void Stepper::disableStepper() const
{
    pwm_set_enabled(sliceNumber, false);
    gpio_put(pins.enable, true);
}

void Stepper::step(float hz, int count)
{
    if (running)
    {
        cancel_repeating_timer(&timerData);
        running = false;
    }
    microStepsToGo = count;
    setRate(hz);
}

void Stepper::addSchedule(time_t from, time_t to, float degrees)
{
    printf("Setting schedule\r\n");
    step(MAX_HZ, 60000);
}

bool Stepper::alarmHandler(repeating_timer_t *timer)
{
    return ((Stepper *)timer->user_data)->handleSpecificAlarm();
}

bool Stepper::handleSpecificAlarm()
{
//    printf("Inner alarm handler, microsteps = %d\r\n", microStepsToGo);
    if (gpio_get_out_level(pins.step))
    {
        gpio_put(pins.step, false);
        --microStepsToGo;
        if (!microStepsToGo)
        {
            disableStepper();

            return false;
        }
    }
    else
    {
        gpio_put(pins.step, true);
    }

    return true;
}
