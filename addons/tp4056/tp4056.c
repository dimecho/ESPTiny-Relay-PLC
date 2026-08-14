#include <stdint.h>

#if defined(ADDON_ESP32S2)
#define GPIO_BASE         0x3f404000
#define GPIO_ENABLE_W1TS  (GPIO_BASE + 0x24)
#define GPIO_OUT_W1TS     (GPIO_BASE + 0x08)
#define GPIO_OUT_W1TC     (GPIO_BASE + 0x0C)
#define LED_PIN           15
#else
#define GPIO_BASE         0x3FF44000
#define GPIO_ENABLE_W1TS  (GPIO_BASE + 0x20)
#define GPIO_OUT_W1TS     (GPIO_BASE + 0x08)
#define GPIO_OUT_W1TC     (GPIO_BASE + 0x0C)
#define LED_PIN           23
#endif

static void delay_loop(volatile uint32_t cycles);

__attribute__((used)) void main(void) {
    volatile uint32_t *gpio_enable = (volatile uint32_t *)GPIO_ENABLE_W1TS;
    volatile uint32_t *gpio_out = (volatile uint32_t *)GPIO_OUT_W1TS;
    volatile uint32_t *gpio_out_clr = (volatile uint32_t *)GPIO_OUT_W1TC;

    *gpio_enable = (1U << LED_PIN);

    for (int i = 0; i < 5; i++) {
        *gpio_out = (1U << LED_PIN);
        delay_loop(400000000);
        *gpio_out_clr = (1U << LED_PIN);
        delay_loop(400000000);
    }
}

static void delay_loop(volatile uint32_t cycles) {
    for (volatile uint32_t i = 0; i < cycles; i++);
}
