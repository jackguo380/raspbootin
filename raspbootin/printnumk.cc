#include <stdint.h>
#include <uart.h>

void printnumk(uint8_t base, uint64_t num) {
    const char digits[] = "0123456789abcdef";

    // Buffer big enough even for uint64 max
    // Even in octal it is only up to 22 digits
    char buf[32];
    int buf_len = 0;

    // always run once incase num == 0
    do {
        // manually specifying the div/modulo value
        // allows the compiler to optimize better
        switch (base) {
            case 8:
                buf[buf_len++] = digits[num % 8];
                num /= 8;
                break;
            case 10:
                buf[buf_len++] = digits[num % 10];
                num /= 10;
                break;
            case 16:
                buf[buf_len++] = digits[num % 16];
                num /= 16;
                break;
            default:
                return;
        }
    } while (num);

    // buf gets the number in reverse
    for (int i = buf_len - 1; i >= 0; i--) {
        UART::putc(buf[i]);
    }
}
