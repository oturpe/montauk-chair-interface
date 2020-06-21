#include <alsa/asoundlib.h>
#include <errno.h>
#include <gpiod.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define GPIO_DEVICE "/dev/gpiochip0"
#define PIN 12
// May be any of these
#define SND_DEVICE_0 "hw:0,0"
#define SND_DEVICE_1 "hw:1,0"
#define SND_DEVICE_2 "hw:2,0"
#define SND_DATA_SIZE 32

// Pwm step length. Given in units of microsecond.
#define PWM_STEP 100
// Pwm cycle length. Given in units of PWM_STEP.
#define PWM_CYCLE 100
// Highest allowed value for pwm_cycle_on
#define PWM_CYCLE_ON_MAX 40

#define LOG_FILE "/var/opt/montauk-chair-interface/montauk-chair-interface.log"
#define LOG_MAX_ROWS (100 * 1000)

struct gpiod_chip* chip;
struct gpiod_line* line;

snd_pcm_t* capture_handle;
snd_pcm_hw_params_t* hw_params;
int snd_rate = 44000;
int16_t* snd_data;

#define UPDATE_RATE 10

 // Log file descriptor
int log_fd;
uint log_row_index = 0;

void log_init() {
    if ((log_fd = open(LOG_FILE, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) < 0) {
        // Opening log file failed. Cannot even log an error.
        exit(1);
    }
}

void log_close() {
    if (close(log_fd) < 0) {
        // Closing log file failed. Perhaps it does not help to try to log
        // something there.
        exit(1);
    }
}

void log_write(char* format, ...) {
    if (log_row_index++ > LOG_MAX_ROWS) {
        // Max log length reached. Close, truncate and open the log file.
        log_row_index = 0;
        if (close(log_fd) < 0) {
            // Closing log file failed. Perhaps it does not help to try to log
            // something there.
            exit(1);
        }
        if ((log_fd = open(LOG_FILE, O_TRUNC|O_WRONLY)) < 0) {
            // Opening log file failed. Cannot even log an error.
            exit(1);
        }
    }

    va_list vargs;
    va_list vargs2;
    va_start(vargs, format);
    va_copy(vargs2, vargs);
    vdprintf(log_fd, format, vargs);
    vprintf(format, vargs2);
    va_end(vargs);
    va_end(vargs2);
}

void gpio_init(void) {
    chip = gpiod_chip_open(GPIO_DEVICE);
    if (!chip) {
        log_write("Could not open gpio chip. Error: %s\n", strerror(errno));
        exit(1);
    }
    line = gpiod_chip_get_line(chip, PIN);
    if (!line) {
        log_write("Could not open gpio line. Error: %s\n", strerror(errno));
        exit(1);
    }

    if (gpiod_line_request_output(line, "montauk-chair-interface", 1) < 0) {
        log_write( 
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

    log_write("Attempting audio device 0.\n");
    err = snd_pcm_open(&capture_handle, SND_DEVICE_0, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        log_write("Could not open audio device 0. Error: %s\n", snd_strerror(err));
        log_write("Attempting audio device 1.\n");
        err = snd_pcm_open(&capture_handle, SND_DEVICE_1, SND_PCM_STREAM_CAPTURE, 0);
    }
    if (err < 0) {
        log_write("Could not open audio device 1. Error: %s\n", snd_strerror(err));
        log_write("Attempting audio device 2.\n");
        err = snd_pcm_open(&capture_handle, SND_DEVICE_2, SND_PCM_STREAM_CAPTURE, 0);
    }
    if (err < 0) {
        log_write("Could not open audio device 2. Error: %s\n", snd_strerror(err));
        log_write("Could not open any audio device.\n");
        exit(1);
    }
    log_write("Audio device opened.\n");

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        log_write(
            "Could not allocate hw parameters. Error: %s\n",
            snd_strerror(err)
        );
        exit(1);
    }

    if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0) {
        log_write(
            "Could not initialize hw parameters. Error: %s\n",
            snd_strerror(err)
        );
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        log_write("Could not set access type. Error: %s\n", snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
        log_write("Could not set sample format. Error: %s\n", snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &snd_rate, 0)) < 0) {
        log_write("Could not set sample rate. Error: %s\n", snd_strerror(err));
        exit (1);
    }
    log_write("Rate set at %d\n", snd_rate);

    if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, 1)) < 0) {
        log_write("Could not set channel count. Error: %s\n", snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
        log_write("Could not set parameters. Error: %s\n", snd_strerror(err));
        exit(1);
    }

    snd_pcm_hw_params_free(hw_params);

    if ((err = snd_pcm_prepare(capture_handle)) < 0) {
        log_write(
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
        log_write("Overrun occurred.\n");
        snd_pcm_prepare(capture_handle);
    }
    else if (rc < 0)
    {
        log_write("error from read: %s\n", snd_strerror(rc));
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
    log_init();
    gpio_init();
    snd_init();

    struct timespec spec;
    unsigned long us;
    unsigned long us_previous = 0;
    int pwm_step = 0;
    int pin_level = 0;
    uint pwm_cycle_on = 0;
    // Constant width null terminated string, content added inside the pwm loop
    char log_entry[PWM_CYCLE + 1];
    log_entry[PWM_CYCLE] = '\0';

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

                log_write("%s\n", log_entry);

                get_pwm_cycle_on(&pwm_cycle_on);
            }
            if (pwm_step == pwm_cycle_on) {
                // Falling edge
                pin_level = 0;
                gpiod_line_set_value(line, pin_level);
            }

            log_entry[pwm_step] = pin_level ? '#' : '.';
        }

    }

    gpio_close();
    log_close();
}
