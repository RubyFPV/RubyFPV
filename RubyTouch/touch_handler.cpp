#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sys/ioctl.h>
#include <string>
#include <thread>
#include <pigpio.h>
#include <chrono>

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 600
#define HOLD_DURATION_MS 50
#define PULSE_DURATION_MS 50

// GPIO Mappings (BCM numbering)
const int GPIO_MAP[3][3] = {
    /* Row 1 */ {24, -1, 22},   // Col 1, 2, 3
    /* Row 2 */ {17, 18, 23},   // Col 1, 2, 3
    /* Row 3 */ {25, -1, 27}    // Col 1, 2, 3
};

bool verbose_mode = false;
const int CELL_WIDTH = SCREEN_WIDTH / 3;
const int CELL_HEIGHT = SCREEN_HEIGHT / 3;

struct TouchState {
    bool active = false;
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    int row = -1;
    int col = -1;
};

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [-v]\n";
    std::cerr << "Options:\n";
    std::cerr << "  -v    Enable verbose debugging output\n";
}

void trigger_gpio(int gpio_pin) {
    if (gpio_pin == -1) return;
    
    gpioWrite(gpio_pin, 1);
    if (verbose_mode) {
        std::cout << "GPIO " << gpio_pin << " ON\n";
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(PULSE_DURATION_MS));
    
    gpioWrite(gpio_pin, 0);
    if (verbose_mode) {
        std::cout << "GPIO " << gpio_pin << " OFF\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (std::string(argv[1]) == "-v") {
            verbose_mode = true;
            std::cout << "Verbose mode enabled\n";
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    // Initialize pigpio
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio initialization failed\n";
        return 1;
    }

    // Initialize all GPIO pins
    const int gpio_pins[] = {17, 18, 22, 23, 24, 25, 27};
    for (int pin : gpio_pins) {
        gpioSetMode(pin, PI_OUTPUT);
        gpioWrite(pin, 0);
    }

    // Open touchscreen device
    const char* touch_device = "/dev/input/event0";
    int fd_touch = open(touch_device, O_RDONLY);
    if (fd_touch < 0) {
        std::cerr << "Error opening touch device: " << strerror(errno) << "\n";
        gpioTerminate();
        return 1;
    }

    // Get touchscreen axis ranges
    input_absinfo abs_info;
    ioctl(fd_touch, EVIOCGABS(ABS_X), &abs_info);
    int touch_min_x = abs_info.minimum;
    int touch_max_x = abs_info.maximum;

    ioctl(fd_touch, EVIOCGABS(ABS_Y), &abs_info);
    int touch_min_y = abs_info.minimum;
    int touch_max_y = abs_info.maximum;

    // Set touch device to non-blocking
    fcntl(fd_touch, F_SETFL, O_NONBLOCK);
    struct input_event ev;
    TouchState touch_state;

    while (true) {
        while (read(fd_touch, &ev, sizeof(ev)) > 0) {
            if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                if (ev.value == 1) {  // Touch down
                    // Get fresh coordinates directly from device
                    ioctl(fd_touch, EVIOCGABS(ABS_X), &abs_info);
                    int current_x = abs_info.value;
                    ioctl(fd_touch, EVIOCGABS(ABS_Y), &abs_info);
                    int current_y = abs_info.value;

                    // Calculate screen position
                    int screen_x = ((current_x - touch_min_x) * SCREEN_WIDTH) / 
                                  (touch_max_x - touch_min_x);
                    int screen_y = ((current_y - touch_min_y) * SCREEN_HEIGHT) / 
                                  (touch_max_y - touch_min_y);

                    // Calculate grid position
                    touch_state.col = (screen_x / CELL_WIDTH) + 1;
                    touch_state.row = (screen_y / CELL_HEIGHT) + 1;

                    // Clamp values
                    touch_state.col = std::max(1, std::min(3, touch_state.col));
                    touch_state.row = std::max(1, std::min(3, touch_state.row));

                    // Update state
                    touch_state.active = true;
                    touch_state.start_time = std::chrono::steady_clock::now();

                    if (verbose_mode) {
                        std::cout << "New touch START at Row " << touch_state.row 
                                << " Col " << touch_state.col 
                                << " [X: " << screen_x << " Y: " << screen_y << "]\n";
                    }
                }
                else if (ev.value == 0) {  // Touch up
                    if (touch_state.active) {
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - touch_state.start_time).count();

                        if (duration >= HOLD_DURATION_MS) {
                            int gpio_pin = GPIO_MAP[touch_state.row-1][touch_state.col-1];
                            if (gpio_pin != -1) {
                                std::thread(trigger_gpio, gpio_pin).detach();
                                if (verbose_mode) {
                                    std::cout << "Triggered Row " << touch_state.row 
                                            << " Col " << touch_state.col 
                                            << " (" << duration << "ms)\n";
                                }
                            }
                        }
                        touch_state.active = false;
                    }
                }
            }
        }
        usleep(100000);
    }


    close(fd_touch);
    gpioTerminate();
    return 0;
}
