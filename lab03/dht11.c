/*!
 * \file      dht11.c
 * \brief     Driver for the DHT11 temperature and humidity sensor, no TIM2.
 */

#include "gpio.h"     // for gpio_set_mode, gpio_set, gpio_get             :contentReference[oaicite:0]{index=0}
#include "platform.h"
#include "uart.h"
#include "dht11.h"
#include "delay.h"

#define DHT11_PIN            PA_0
#define START_SIGNAL_MS      20       //!< pull low =18 ms
#define RELEASE_HIGH_US      40       //!< host pulls high, then waits 20–40 µs
#define RESPONSE_TIMEOUT_US  200      //!< wait up to 200 µs for each transition
#define BIT_SAMPLE_US        40       //!< sample in middle of bit (˜40 µs)

// wait until pin == level, or timeout in µs expires
static int wait_pin_state(Pin pin, int level, uint32_t timeout_us) {
    while (gpio_get(pin) != level) {
        if (timeout_us-- == 0) return -1;
        delay_us(1);
    }
    return 0;
}

int dht11_poll(uint8_t* data_out) {
    // 1) Send start pulse
    gpio_set_mode(DHT11_PIN, Output);
    gpio_set(DHT11_PIN, 0);
    delay_ms(START_SIGNAL_MS);

    // 2) Release bus: drive high, wait, then switch to input+pull-up
    gpio_set(DHT11_PIN, 1);
    delay_us(RELEASE_HIGH_US);
    gpio_set_mode(DHT11_PIN, Input);

    // 3) Sensor response: LOW ~80µs, then HIGH ~80µs, then LOW ? data
    if (wait_pin_state(DHT11_PIN, 0, RESPONSE_TIMEOUT_US) < 0) {
        uart_print("ERR: no initial LOW\r\n");
        return -1;
    }
    if (wait_pin_state(DHT11_PIN, 1, RESPONSE_TIMEOUT_US) < 0) {
        uart_print("ERR: no initial HIGH\r\n");
        return -1;
    }
    if (wait_pin_state(DHT11_PIN, 0, RESPONSE_TIMEOUT_US) < 0) {
        uart_print("ERR: start bit not ending\r\n");
        return -1;
    }

    // 4) Read 40 bits
    for (int i = 0; i < 5; i++) data_out[i] = 0;
    for (int i = 0; i < 40; i++) {
        // wait for line to go HIGH (start of bit)
        if (wait_pin_state(DHT11_PIN, 1, RESPONSE_TIMEOUT_US) < 0) {
            uart_print("ERR: bit start timeout\r\n");
            return -1;
        }
        // sample in middle of the pulse
        delay_us(BIT_SAMPLE_US);
        int bit = gpio_get(DHT11_PIN);
        data_out[i/8] = (data_out[i/8] << 1) | (bit ? 1 : 0);
        // wait for line to go LOW (end of bit)
        if (wait_pin_state(DHT11_PIN, 0, RESPONSE_TIMEOUT_US) < 0) {
            uart_print("ERR: bit end timeout\r\n");
            return -1;
        }
    }

    // 5) Checksum
    uint8_t sum = data_out[0] + data_out[1] + data_out[2] + data_out[3];
    if (sum != data_out[4]) {
        uart_print("ERR: checksum mismatch\r\n");
        return -1;
    }

    uart_print("OK: DHT11 read complete\r\n");
    return 0;
}
