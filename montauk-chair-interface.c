#include <errno.h>
#include <gpiod.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/gpiochip0"
#define PIN 12

// Pwm step length. Given in units of microsecond.
#define PWM_STEP 1000
// Pwm cycle length. Given in units of PWM_STEP.
#define PWM_CYCLE 20
// Pwm cycle 'on' part length. Given in units of PWM_STEP.
#define PWM_CYCLE_ON 6

struct gpiod_chip* chip;
struct gpiod_line* line;

void gpio_init(void) {
    chip = gpiod_chip_open(DEVICE_PATH);
    if (!chip) {
        printf("Could not open gpio chip. Error: %s\n", strerror(errno));
        exit(-1);
    }
    line = gpiod_chip_get_line(chip, PIN);
    if (!line) {
        printf("Could not open gpio line. Error: %s\n", strerror(errno));
        exit(-1);
    }

    if (!gpiod_line_request_output(line, "montauk-chair-interface", 1)) {
        printf(
            "Could not set gpio line to output. Error: %s\n", 
            strerror(errno)
        );
        exit(-1);
    }
}

void gpio_close() {
    gpiod_chip_close(chip);
}

int main() {
    //gpio_init();

    struct timespec spec;
    unsigned long us;
    unsigned long usPrevious = 0;
    int pwmStep = 0;
    int pinLevel = 1;
    while(1) {
        clock_gettime(CLOCK_MONOTONIC, &spec);
        us = 1e6 * spec.tv_sec + spec.tv_nsec / 1e3;

        if (us > usPrevious + PWM_STEP) {
            usPrevious = us;
            pwmStep++;
            printf("%d", pinLevel);
            if (pwmStep == PWM_CYCLE_ON) {
                // Falling edge
                pinLevel = 0;
                //gpiod_line_set_value(line, pinLevel);
            }
            if (pwmStep == PWM_CYCLE) {
                // Rising edge
                pinLevel = 1;
                pwmStep = 0;
                //gpiod_line_set_value(line, pinLevel);
                printf("\n");
            }
        }
    }

    gpio_close();
}
