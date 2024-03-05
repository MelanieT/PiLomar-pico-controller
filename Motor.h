//
// Created by Melanie on 17/02/2024.
//

#ifndef PILOMAR_MOTOR_H
#define PILOMAR_MOTOR_H

#include <pico/time.h>
#include "list"

struct Pins
{
    int step;
    int dir;
    int enable;
};

struct Options
{
    bool autoPowerOff = false;
    void *userData = nullptr;
    bool (*callback)(void *userData) = nullptr;
    bool reverse = false;
    int endstop = -1;
    bool dirToEndstop = false;
    int microsteps = 8;
};

class Motor
{
public:
    Motor(Pins motor, Options options, int microStepsPerRevolution, int maxSteps);
    ~Motor();

    void setOptions(Options options);
    Options getOptions();
    void home();
    void runToTarget(double targetSpeed, int target);
    bool isRunning();
    void disableMotor() const;
    void setCurrentPosition(int position);
    [[nodiscard]] int getCurrentPosition() const;

protected:
    unsigned sliceNumber;

private:
    enum State
    {
        Stopped,
        Running,
        Homing,
        MovingOffEndstop,
        BackingUp
    };

    volatile State state = Stopped;         // State of motor
    volatile double speed = 0;              // Current speed
    volatile bool direction = false;        // Current direction
    volatile bool homed = false;            // true if motor has been homed
    volatile int position = 0;              // Current absolute position
    volatile int stepsToGo = 0;             // Motor steps to go on this plan step

    struct
    {
        int steps;
        double speed;
        bool direction;
        int expectedPosition;
    } plan[1024] = {};
    int step = 0;

    int microStepsPerRevolution;
    int maxSteps;
    Pins pins{};
    Options options;

    static std::list<Motor *> instances;

    static bool initialized;
    repeating_timer_t timerData{};

    void setPwmMode() const;
    void setGpioMode() const;
    void enableMotor() const;
    void setDirection(bool forward);
    [[nodiscard]] int motorSpeedStepDeltaSteps(int delta) const;
    void motorSpeedStep();
    void runStepper(double newSpeed, bool newDirection);
    void setPwmFreq(float freq) const;
    bool performStep();
    static void interruptHandler();
    void handleSpecificInterrupt();

    static bool alarmHandler(repeating_timer_t *t);
    bool handleSpecificAlarm();

};


#endif //PILOMAR_MOTOR_H
