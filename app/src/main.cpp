#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>

// Wrap hardware preprocessor definitions safely in a typed C++ abstraction
class StatusLed {
private:
    const gpio_dt_spec spec;
    bool state{false};
public:
    // Pull LED configuration directly from Devicetree 'led0' node
    explicit StatusLed(const gpio_dt_spec &spec) : spec(spec) {}

    bool init() {
        if (!gpio_is_ready_dt(&spec)) {
            return false;
        }
        return gpio_pin_configure_dt(&spec, GPIO_OUTPUT_ACTIVE) == 0;
    }

    void toggle() {
        gpio_pin_toggle_dt(&spec);
        state = !state;
    }

    bool isHigh() const { return state; }
};

// Grab hardware binding at compile time
static constexpr gpio_dt_spec led_spec = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main()
{
    StatusLed led(led_spec);

    if (!led.init()) {
        printk("Error: LED device not ready\n");
        return 0;
    }

    printk("C++ Zephyr App Started on %s!\n", CONFIG_BOARD);

    while (true) {
        led.toggle();
        k_msleep(1000);
    }
}