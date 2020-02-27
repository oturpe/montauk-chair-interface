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

    if (gpiod_line_request_output(line, "montauk-chair-interface", 1) < 0) {
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
    gpio_init();

    struct timespec spec;
    unsigned long us;
    unsigned long us_previous = 0;
    int pwm_step = 0;
    int pin_level = 1;
    uint pwm_cycle_on = 0;
    while(1) {
        clock_gettime(CLOCK_MONOTONIC, &spec);
        us = 1e6 * spec.tv_sec + spec.tv_nsec / 1e3;

        if (us > us_previous + PWM_STEP) {
            us_previous = us;
            pwm_step++;
            printf("%d", pin_level);
            if (pwm_step == pwm_cycle_on) {
                // Falling edge
                pin_level = 0;
                gpiod_line_set_value(line, pin_level);
            }
            if (pwm_step == PWM_CYCLE) {
                // Rising edge
                pin_level = 1;
                pwm_step = 0;
                gpiod_line_set_value(line, pin_level);
                printf("\n");
                pwm_cycle_on = (pwm_cycle_on+1) % PWM_CYCLE;
            }
        }
    }

    gpio_close();
}
