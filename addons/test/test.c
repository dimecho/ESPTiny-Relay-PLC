#include <stdint.h>

#if defined(ADDON_ESP32S2)
#define UART0_BASE 0x3F400000
#else
#define UART0_BASE 0x3FF40000
#endif
#define UART_FIFO_REG (UART0_BASE + 0x00)
#define UART_STATUS_REG (UART0_BASE + 0x1C)
#define UART_TXFIFO_CNT_MASK 0x1FF

static void uart_putc(char c) {
    while (((*(volatile uint32_t *)UART_STATUS_REG) & UART_TXFIFO_CNT_MASK) >= 0x7F) {}
    *(volatile uint32_t *)UART_FIFO_REG = (uint32_t)c;
}

__attribute__((used)) void main() {
    uart_putc('a');
    uart_putc('d');
    uart_putc('d');
    uart_putc('o');
    uart_putc('n');
    uart_putc(' ');
    uart_putc('o');
    uart_putc('k');
    uart_putc('\n');
}
