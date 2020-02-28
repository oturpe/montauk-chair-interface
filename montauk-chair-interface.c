#include <alsa/asoundlib.h>
#include <errno.h>
#include <gpiod.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define GPIO_DEVICE "/dev/gpiochip0"
#define PIN 12
#define SND_DEVICE "hw:2,0"
#define SND_DATA_SIZE 32

// Pwm step length. Given in units of microsecond.
#define PWM_STEP 100
// Pwm cycle length. Given in units of PWM_STEP.
#define PWM_CYCLE 100
// Highest allowed value for PWM_CYCLE
#define PWM_CYCLE_ON_MAX 40

struct gpiod_chip* chip;
struct gpiod_line* line;

snd_pcm_t* capture_handle;
snd_pcm_hw_params_t* hw_params;
int snd_rate = 44000;
int16_t* snd_data;

#define UPDATE_RATE 10


void gpio_init(void) {
    chip = gpiod_chip_open(GPIO_DEVICE);
    if (!chip) {
        printf("Could not open gpio chip. Error: %s\n", strerror(errno));
        exit(1);
    }
    line = gpiod_chip_get_line(chip, PIN);
    if (!line) {
        printf("Could not open gpio line. Error: %s\n", strerror(errno));
        exit(1);
    }

    if (gpiod_line_request_output(line, "montauk-chair-interface", 1) < 0) {
        printf(
            "Could not set gpio line to output. Error: %s\n",
            strerror(errno)
        );
        exit(1);
    }
}

void gpio_close() {
    gpiod_chip_close(chip);
}

void snd_init() {
    int err;

    if ((err = snd_pcm_open(&capture_handle, SND_DEVICE, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        printf("Could not open audio device. Error: %s\n", snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) <0) {
        printf(
            "Could not allocate hw parameters. Error: %s\n",
            snd_strerror(err)
        );
        exit(1);
    }

    if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0) {
        printf(
            "Could not initialize hw parameters. Error: %s\n",
            snd_strerror(err)
        );
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        printf("Could not set access type. Error: %s\n", snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
        printf("Could not set sample format. Error: %s\n", snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &snd_rate, 0)) < 0) {
        printf (
            "Could not set sample rate. Error: %s\n",
            snd_strerror(err)
        );
        exit (1);
    }
    printf("Rate set at %d\n", snd_rate);

    if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, 1)) < 0) {
        printf("Could not set channel count. Error: %s\n", snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
        printf("Could not set parameters. Error: %s\n", snd_strerror(err));
        exit(1);
    }

    snd_pcm_hw_params_free(hw_params);

    if ((err = snd_pcm_prepare(capture_handle)) < 0) {
        printf(
            "Could not prepare audio interface for use. Error: %s\n",
            snd_strerror(err)
        );
        exit(1);
    }

    snd_data = (uint16_t *)malloc(SND_DATA_SIZE);
}

void get_pwm_cycle_on(uint* pwm_cycle_on) {
    static uint32_t offset = 0;
    int rc = snd_pcm_readi(capture_handle, snd_data + offset, 1);
    if (rc == -EPIPE)
    {
        printf("Overrun occurred.\n");
        snd_pcm_prepare(capture_handle);
    }
    else if (rc < 0)
    {
        fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
    }

    static uint32_t update_counter = 0;
    update_counter = (update_counter + 1) % UPDATE_RATE;
    if (update_counter > 0) {
        return;
    }

    uint32_t prev_offset = offset;
    offset = (offset + 1) % SND_DATA_SIZE;
    if (snd_data[offset] > snd_data[prev_offset]) {
        if (*pwm_cycle_on < PWM_CYCLE_ON_MAX) {
            (*pwm_cycle_on)++;
        }
    }
    else if (snd_data[offset] < snd_data[prev_offset]) {
        if (*pwm_cycle_on > 0) {
            (*pwm_cycle_on)--;
        }
    }
}

int main() {
    gpio_init();
    snd_init();

    struct timespec spec;
    unsigned long us;
    unsigned long us_previous = 0;
    int pwm_step = 0;
    int pin_level = 0;
    uint pwm_cycle_on = 0;
    while(1) {
        clock_gettime(CLOCK_MONOTONIC, &spec);
        us = 1e6 * spec.tv_sec + spec.tv_nsec / 1e3;

        if (us < us_previous) {
            // us overflow, rough recovery here
            us_previous = 0;
        }
        if (us > us_previous + PWM_STEP) {
            us_previous = us;
            pwm_step = (pwm_step + 1) % PWM_CYCLE;
            if (pwm_step == 0) {
                // Rising edge
                pin_level = 1;
                gpiod_line_set_value(line, pin_level);
                printf("\n");

                get_pwm_cycle_on(&pwm_cycle_on);
            }
            if (pwm_step == pwm_cycle_on) {
                // Falling edge
                pin_level = 0;
                gpiod_line_set_value(line, pin_level);
            }

            printf("%d", pin_level);
        }

    }

    gpio_close();
}
