// SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
// SPDX-License-Identifier: BSD-3-Clause

// This is the highest wrap resulting in an integral frequency, it represents 1/2 full step per second
#define MAX_WRAP 62500.0

#define MIN_HZ 10

#define SPEED_STEP (31 * options.microsteps)
#define SPEED_STEP_PULSES (5 * options.microsteps)

#define DIV_MIN ((0x01 << 4) + 0x0)

#include <cstdio>
#include <hardware/pwm.h>
#include <algorithm>
#include <hardware/regs/intctrl.h>
#include <hardware/irq.h>
#include <hardware/gpio.h>
#include "Motor.h"

bool Motor::initialized = false;
std::list<Motor *> Motor::instances;

Motor::Motor(Pins pins, Options options, int microStepsPerRevolution, int maxSteps)
{
    this->pins = pins;
    this->options = options;

    this->microStepsPerRevolution = microStepsPerRevolution;
    this->maxSteps = maxSteps;

//    printf("Pins: step %d dir %d enable %d\r\n", this->pins.step, this->pins.dir, this->pins.enable);
//    printf("Max steps %d\r\n", maxSteps);

    gpio_set_function(pins.step, GPIO_FUNC_SIO);
    gpio_set_dir(pins.step, true);
    gpio_put(pins.step, false);
    gpio_disable_pulls(pins.step);

    gpio_set_function(pins.dir, GPIO_FUNC_SIO);
    gpio_set_dir(pins.dir, true);
    gpio_put(pins.dir, false);
    gpio_disable_pulls(pins.dir);

    gpio_set_function(pins.enable, GPIO_FUNC_SIO);
    gpio_set_dir(pins.enable, true);
    gpio_put(pins.enable, true);
    gpio_disable_pulls(pins.enable);

    if (options.endstop != -1)
    {
        gpio_set_function(options.endstop, GPIO_FUNC_SIO);
        gpio_set_dir(options.endstop, false);
        gpio_pull_up(options.endstop);
    }

    sliceNumber = pwm_gpio_to_slice_num(pins.step);

    if (std::find_if(Motor::instances.begin(), Motor::instances.end(),
                     [this](const Motor *motor) {return motor->sliceNumber == this->sliceNumber;}) != Motor::instances.end())
    {
        printf("Duplicate slices are not supported\r\n");
        return; // Should throw here but that is disabled on Pico
    }

    Motor::instances.emplace_back(this);

    if (!initialized)
    {
        irq_set_exclusive_handler(PWM_IRQ_WRAP, &Motor::interruptHandler);
        irq_set_enabled(PWM_IRQ_WRAP, true);

        initialized = true;
    }
    pwm_set_irq_enabled(sliceNumber, true);

    pwm_set_clkdiv_mode(sliceNumber, PWM_DIV_FREE_RUNNING);
    pwm_set_clkdiv_int_frac(sliceNumber, 250, 0);

    if (!homed)
        home();

    add_repeating_timer_ms(1000, alarmHandler, this, &timerData);

    while (!homed)
        sleep_ms(10);
}

Motor::~Motor()
{
    Motor::instances.remove(this);
}

void Motor::setPwmFreq(float freq) const {
    uint32_t clock = 125000000;
    auto div = (uint32_t)((double)(clock << 4) / freq / MAX_WRAP);
    if (div < DIV_MIN) {
        div = DIV_MIN;
    }
    if (div > 254 * 16)
        div = 256 * 16;
    auto top = (uint32_t)(((double)(clock << 4) / div / freq) - 1);
    float out = (float)(clock << 4) / (float)div / (float)(top + 1);

    // Some code useful for debugging this. Uncomment as needed
//    printf("Freq = %f, ",         freq);
//    printf("Top = %ld, ",         top);
//    printf("Div = %.2f, ", (float)div/16);
//    printf("Out = %f\n",          out);

    pwm_set_wrap(sliceNumber, top);
    pwm_set_chan_level(sliceNumber, PWM_CHAN_A, div << 4);
    pwm_set_clkdiv_int_frac(sliceNumber, div >> 4, div & 0x0f);
    pwm_set_enabled(sliceNumber, true);
}

void Motor::interruptHandler()
{
    for (int slice = 0; slice < 8; slice++)
    {
        if (pwm_get_irq_status_mask() & (1 << slice))
        {
            auto it = std::find_if(Motor::instances.begin(), Motor::instances.end(),
                                   [slice](Motor *motor) {return motor->sliceNumber == slice;});
            if (it != Motor::instances.end())
            {
                (*it)->handleSpecificInterrupt();
            }
            pwm_clear_irq(slice);
        }
    }
}

void Motor::handleSpecificInterrupt()
{
    if (state != Running)
    {
//        if (!gpio_get(pins.endstop))
//        {
//            pwm_set_enabled(sliceNumber, false);
//            stepsToGo = 0;
//            homed = true;
//            position = 0;
//            state = Stopped;
//
//            return;
//        }

        position++;

        if (state == MovingOffEndstop)
        {
            if (gpio_get(options.endstop))
            {
                state = Homing;
                setDirection(options.dirToEndstop);
            }
        }
        if (state == Homing)
        {
            if (!gpio_get(options.endstop))
            {
                state = BackingUp;
                setDirection(!options.dirToEndstop);
                position = -125 * options.microsteps;
            }
        }
        if (state == BackingUp)
        {
            if (position >= 0)
            {
                pwm_set_enabled(sliceNumber, false);
                state = Stopped;
                homed = true;
                if (options.callback != nullptr)
                {
                    if (options.callback(options.userData))
                        disableMotor();
                }
                else
                {
                    if (options.autoPowerOff)
                        disableMotor();
                }
            }
        }
        return;
    }

    if (options.endstop != -1)
    {
        if (!gpio_get(options.endstop))
        {
            // HALP! Hit endstop in normal run
            pwm_set_enabled(sliceNumber, false);
            state = Stopped;
            position = 0;
        }
    }

    performStep();

    if (stepsToGo)
    {
        if (!--stepsToGo)
            motorSpeedStep();
    }
}

bool Motor::alarmHandler(repeating_timer_t *timer)
{
    return ((Motor *)timer->user_data)->handleSpecificAlarm();
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-convert-member-functions-to-static"
#pragma ide diagnostic ignored "readability-make-member-function-const"
bool Motor::handleSpecificAlarm()
{
    if (gpio_get_out_level(pins.step))
    {
        gpio_put(pins.step, false);

        if (!performStep())
            return false;

        if (stepsToGo)
        {
            if (!--stepsToGo)
            {
                motorSpeedStep();
                return false;
            }
        }
    }
    else
    {
        gpio_put(pins.step, true);
    }

    return true;
}
#pragma clang diagnostic pop

void Motor::home()
{
    if (options.endstop == -1)
    {
        homed = true;
        if (!options.autoPowerOff && options.callback == nullptr)
            enableMotor();
        return;
    }

    setPwmMode();

    if (!gpio_get(options.endstop) && state != BackingUp)
    {
//        printf("Move off endstop\n");
        setDirection(!options.dirToEndstop);
        state = MovingOffEndstop;
        setPwmFreq(250.0f * (float)options.microsteps);

        enableMotor();

        return;
    }

//    printf("Home\n");
    state = Homing;

    setPwmMode();
    setDirection(options.dirToEndstop);
    setPwmFreq(250.0f * (float)options.microsteps);

    enableMotor();
}

void Motor::setPwmMode() const
{
    gpio_set_function(pins.step, GPIO_FUNC_PWM);
}

void Motor::setGpioMode() const
{
    gpio_put(pins.step, false);
    gpio_set_function(pins.step, GPIO_FUNC_SIO);
}

void Motor::enableMotor() const
{
    gpio_put(pins.enable, false);
}

void Motor::disableMotor() const
{
    pwm_set_enabled(sliceNumber, false);
    gpio_put(pins.enable, true);
}

void Motor::setDirection(bool forward)
{
    direction = forward;
    gpio_put(pins.dir, forward ^ options.reverse);
}

int Motor::motorSpeedStepDeltaSteps(int delta) const
{
    return (delta + (SPEED_STEP - 1)) / SPEED_STEP;
}

void Motor::motorSpeedStep()
{
    for (;;)
    {
        if (plan[step].speed < 0)
        {
            stepsToGo = 0;
            if (options.callback != nullptr)
            {
                if (options.callback(options.userData))
                    disableMotor();
            }
            if (options.autoPowerOff)
                disableMotor();
            return;
        }

        runStepper(plan[step].speed, plan[step].direction);
        if (plan[step].steps != 0)
        {
//            printf("Expected %d, actual %d\r\n", plan[step].expectedPosition, position);
            stepsToGo = plan[step].steps;
            step++;
            return;
        }
        step++;
    }
}

void Motor::runStepper(double newSpeed, bool newDirection)
{
//    printf("Run stepper %d, %s\n", newSpeed, newDirection ? "out" : "in");

    if (newSpeed == 0)
    {
        pwm_set_enabled(sliceNumber, false);
        state = Stopped;
        newSpeed = 0;

        return;
    }

    pwm_set_enabled(sliceNumber, false);
    enableMotor();

    speed = newSpeed;
    direction = newDirection;

    setDirection(newDirection);

    if (newSpeed < MIN_HZ)
    {
        setGpioMode();

        int period = (int)(1000000.0 / (newSpeed * 2.0));

        enableMotor();

        gpio_put(pins.step, false);
        add_repeating_timer_us(period, alarmHandler, this, &timerData);
    }
    else
    {
        setPwmMode();
        setPwmFreq((float) newSpeed);
    }

    state = Running;
}

void Motor::runToTarget(double targetSpeed, int target)
{
    if (state != Stopped && state != Running)
        return;

    int plan_step = 0;

    if (target < 0.001)
        target = 0;

    if (target > maxSteps)
        target = maxSteps;

    setPwmMode();

    int motor_position = position;
    int orig_position = position;

    int dist = target - position;
    bool newDirection = dist > 0;

    if (position == target && state != Running) // No motion in the ocean
        return;

    stepsToGo = 0;
    double newSpeed = speed;

    if (state == Running) // We are already moving
    {
        if (newDirection != direction) // But the wrong way!
        {
            do
            {
                newSpeed -= SPEED_STEP;
                if (newSpeed < 0)
                    newSpeed = 0;
                plan[plan_step].expectedPosition = motor_position;
                plan[plan_step].direction = direction;
                plan[plan_step].steps = newSpeed > 0 ? SPEED_STEP_PULSES : 0;
                motor_position = direction ? motor_position + plan[plan_step].steps : motor_position - plan[plan_step].steps;
                plan[plan_step++].speed = newSpeed;
            } while (newSpeed > 0.0);

            direction = newDirection;
        }
    }

    // Get the distance we have left to move
    //printf("target = %d\n", target);
    //printf("motor_position = %d\n", motor_position);
    dist = target - motor_position;
    newDirection = dist > 0;
    dist = abs(dist);

    if (dist == 0) // Hit the nail on the head
    {
        plan[plan_step].expectedPosition = motor_position;
        plan[plan_step].direction = direction;
        plan[plan_step].steps = 0;
        plan[plan_step++].speed = -1;
    }
    else
    {
        int maxsteps = dist / SPEED_STEP_PULSES;
        int ramp_steps = motorSpeedStepDeltaSteps((int)newSpeed);
        if (maxsteps < ramp_steps) // Not even enough room to brake
        {
            while (newSpeed > 0) // Generate overshoot
            {
                newSpeed -= SPEED_STEP;
                if (newSpeed < 0)
                    newSpeed = 0;
                plan[plan_step].expectedPosition = motor_position;
                plan[plan_step].direction = newDirection;
                plan[plan_step].steps = newSpeed > 0 ? SPEED_STEP_PULSES : 0;
                motor_position = newDirection ? motor_position + plan[plan_step].steps : motor_position - plan[plan_step].steps;
                plan[plan_step++].speed = newSpeed;
            }

            dist = target - motor_position;
            newDirection = dist > 0;
            dist = abs(dist);
        }

        dist = target - motor_position;
        newDirection = dist > 0;
        dist = abs(dist);
        maxsteps = dist / SPEED_STEP_PULSES;
        ramp_steps = motorSpeedStepDeltaSteps((int)newSpeed);
        int steps_left = maxsteps - ramp_steps;

        double max_speed = (int)newSpeed + SPEED_STEP * ((double)steps_left / 2);
        if (max_speed > targetSpeed)
            max_speed = targetSpeed;

        if (maxsteps < 5) // Less than 250 units
        {
            plan[plan_step].expectedPosition = motor_position;
            plan[plan_step].direction = newDirection;
            plan[plan_step].steps = dist;
            motor_position = newDirection ? motor_position + plan[plan_step].steps : motor_position - plan[plan_step].steps;
            plan[plan_step++].speed = 200; // Constant direct move

            plan[plan_step].expectedPosition = motor_position;
            plan[plan_step].direction = newDirection;
            plan[plan_step].steps = 0;
            plan[plan_step++].speed = 0;
        }
        else
        {
            if (max_speed <= newSpeed)
            {
                ramp_steps = motorSpeedStepDeltaSteps((int)(newSpeed - max_speed));
                while (newSpeed > max_speed)
                {
                    newSpeed -= SPEED_STEP;
                    if (newSpeed < max_speed)
                        newSpeed = max_speed;

                    plan[plan_step].expectedPosition = motor_position;
                    plan[plan_step].direction = newDirection;
                    plan[plan_step].steps = SPEED_STEP_PULSES;
                    motor_position = newDirection ? motor_position + plan[plan_step].steps : motor_position - plan[plan_step].steps;
                    plan[plan_step++].speed = newSpeed; // Constant direct move
                }
            }
            else
            {
                ramp_steps = motorSpeedStepDeltaSteps((int)(max_speed - newSpeed));
                ramp_steps += motorSpeedStepDeltaSteps((int)max_speed);
                while (newSpeed < max_speed)
                {
                    newSpeed += SPEED_STEP;
                    if (newSpeed > max_speed)
                        newSpeed = max_speed;

                    plan[plan_step].expectedPosition = motor_position;
                    plan[plan_step].direction = newDirection;
                    plan[plan_step].steps = SPEED_STEP_PULSES;
                    motor_position = newDirection ? motor_position + plan[plan_step].steps : motor_position - plan[plan_step].steps;
                    plan[plan_step++].speed = newSpeed; // Constant direct move
                }
            }

            // Code for debugging the travel planner
            //printf("speed = %d\n", speed);
            //printf("dist = %d\n", dist);
            //printf("maxsteps = %d\n", maxsteps);
            //printf("ramp_steps = %d\n", ramp_steps);
            //printf("steps_left = %d\n", steps_left);
            //printf("max_speed = %d\n", max_speed);

            ramp_steps--; // Last is zero speed, it will not create any motion, so don't consider it for distance

            plan[plan_step].expectedPosition = motor_position;
            plan[plan_step].direction = newDirection;
            plan[plan_step].steps = dist - ramp_steps * SPEED_STEP_PULSES; // Coasting distance
            motor_position = newDirection ? motor_position + plan[plan_step].steps : motor_position - plan[plan_step].steps;
            plan[plan_step++].speed = newSpeed; // Constant direct move

            while (newSpeed > 0.0)
            {
                newSpeed -= SPEED_STEP;
                if (newSpeed < 0)
                    newSpeed = 0;

                plan[plan_step].expectedPosition = motor_position;
                plan[plan_step].direction = newDirection;
                plan[plan_step].steps = newSpeed > 0 ? SPEED_STEP_PULSES : 0;
                motor_position = newDirection ? motor_position + plan[plan_step].steps : motor_position - plan[plan_step].steps;
                plan[plan_step++].speed = newSpeed;
            }
        }

        plan[plan_step].expectedPosition = motor_position;
        plan[plan_step].direction = direction;
        plan[plan_step].steps = 0;
        plan[plan_step++].speed = -1;
    }

    // Code to dump the finished acceleration plan
//    int i;
//
//    printf("                Steps Speed Dir Pos\n");
//    for (i = 0 ; i < plan_step ; i++)
//    {
//        orig_position = plan[i].direction ? orig_position + plan[i].steps : orig_position - plan[i].steps;
//        printf("Plan step %3d : %5d %8.2lf %-3.3s %5d\n", i + 1, plan[i].steps, plan[i].speed, plan[i].direction ? "out" : "in", orig_position);
//    }

    step = 0;

    motorSpeedStep();
}

bool Motor::isRunning()
{
    return state == Running;
}

void Motor::setCurrentPosition(int newPosition)
{
    if (isRunning())
        return;

    this->position = newPosition;
}

void Motor::setOptions(Options newOptions)
{
    if (isRunning())
        return;

    this->options = newOptions;
}

Options Motor::getOptions()
{
    return options;
}

int Motor::getCurrentPosition() const
{
    return position;
}

bool Motor::performStep()
{
    if (direction)
    {
        position++;
        if (position > maxSteps)
        {
            return false;
        }
    }
    else
    {
        position--;
        if (position <= 0)
        {
            return false;
        }
    }

    return true;
}

