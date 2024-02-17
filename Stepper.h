//
// Created by Melanie on 04/02/2024.
//

#ifndef PILOMAR_STEPPER_H
#define PILOMAR_STEPPER_H

#include <pico/time.h>
#include "list"
#include "time.h"

struct Pins
{
    int step;
    int dir;
    int enable;
    int endstop;
};

class Stepper
{
public:
    Stepper(Pins pins, int microStepsPerDegree);
    ~Stepper();

    void addSchedule(time_t from, time_t to, float degrees);

protected:
    unsigned sliceNumber;

private:
    static int hzToWrap(int hz);
    void handleSpecificInterrupt();
    bool handleSpecificAlarm();
    void setRate(float hz);
    void disableStepper() const;
    void step(float hz, int count);

    static bool initialized;

    static void interruptHandler();
    static bool alarmHandler(repeating_timer_t *t);
    static std::list<Stepper *> instances;

    Pins pins{};
    int microStepsPerDegree;
    int microStepsToGo = 0;
    bool running = false;

    bool rampingUp;
    bool rampingDown;

    int currentHz;
    int targetHz;

    int rampCounter;
    int rampSteps;

    repeating_timer_t timerData{};
};


#endif //PILOMAR_STEPPER_H
