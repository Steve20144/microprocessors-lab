/*!
 * \file      dht11.c
 * \brief     Driver for the DHT11 temperature and humidity sensor, no TIM2.
 */

#include "gpio.h"
#include "platform.h"
#include "uart.h"
#include "dht11.h"
#include "delay.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#define DHT11_PIN            PA_0
#define START_SIGNAL_MS      20       // Minimum 18ms
#define RELEASE_HIGH_US      40       // 20-40µs
#define RESPONSE_TIMEOUT_US  200      // Max wait for transitions
#define BIT_SAMPLE_US        40       // Sample in middle of bit
#define MAX_RETRIES         1        // Number of read attempts
char buf1[64];

bool gpio_read(Pin pin) {
	return gpio_get(pin);
}
// wait until pin == level, or timeout in µs expires
static int wait_pin_state(Pin pin, int level, uint32_t timeout_us) {

    while (gpio_read(pin) != level) {
//			sprintf(buf1,"gpio: %d\r\n", gpio_read(pin));
//			uart_print(buf1);
        if (timeout_us-- == 0) return -1;
        delay_us(1);
    }
//		sprintf(buf1,"pin: %d\tlevel: %d\ttimeout: %d\r\n",gpio_get(pin),level,(RESPONSE_TIMEOUT_US-timeout_us));
//		uart_print(buf1);
    return 0;
}

int dht11_poll(uint8_t* data_out) {
    int retry = 0;
    delay_ms(3000);
		uint8_t bit_array[40];
		uint8_t byte_array[5];

    while (retry++ < MAX_RETRIES) {
        // 1) Send start pulse
        gpio_set_mode(DHT11_PIN, Output);
        gpio_set(DHT11_PIN, 0);
        delay_ms(START_SIGNAL_MS);

        // 2) Release bus and switch to input
        gpio_set(DHT11_PIN, 1);
				//gpio_set_mode(DHT11_PIN, Input);
        delay_us(RELEASE_HIGH_US);
        gpio_set_mode(DHT11_PIN, Input);
//			int del=0;
//			while(del<500) {
//				
//				sprintf(buf1,"gpio on %dus: %d\r\n", del, gpio_read(DHT11_PIN));
//				uart_print(buf1);
//				del = del + 10;
//				delay_us(del);
//			}
        // 3) Wait for sensor response
			//delay_us(40);
        if (wait_pin_state(DHT11_PIN, 0, RESPONSE_TIMEOUT_US) < 0) {
            uart_print("DHT11: No response (low)\r\n");
					continue;
            
        }
			//delay_us(80);
        if (wait_pin_state(DHT11_PIN, 1, RESPONSE_TIMEOUT_US) < 0) {
            uart_print("DHT11: No response (high)\r\n");
					continue;
            
        }
			//while(gpio_read(DHT11_PIN) != 0) {
				//wait for pin to go high
			//}
        if (wait_pin_state(DHT11_PIN, 0, RESPONSE_TIMEOUT_US) < 0) {
            uart_print("DHT11: Start bit not ending\r\n");
					continue;
            
        }
//			while(gpio_read(DHT11_PIN) != 1) {
//							//wait for pin to go low
//						}
        // 4) Read 40 bits (5 bytes)
        for (int i = 0; i < 5; i++) data_out[i] = 0;
        
        for (int i = 0; i < 40; i++) {
            // Wait for start of bit
            if (wait_pin_state(DHT11_PIN, 1, RESPONSE_TIMEOUT_US) < 0) {
                uart_print("DHT11: Bit start timeout\r\n");
                break;
            }
            
            // Sample in middle of the pulse
            delay_us(BIT_SAMPLE_US);
            uint8_t bit = gpio_read(DHT11_PIN);
						//sprintf(buf1,"bit #%d: %d\r\n", i, bit);
						//uart_print(buf1);
						//data_out[i/8] = (data_out[i/8] << 1) | bit;
            bit_array[i]=bit;
						//data_out[i/8] = (data_out[i/8] << 1) | (bit ? 1 : 0);
            
            // Wait for end of bit
            if (wait_pin_state(DHT11_PIN, 0, RESPONSE_TIMEOUT_US) < 0) {
                uart_print("DHT11: Bit end timeout\r\n");
                break;
            }
        }

        // 5) Verify checksum
        uint8_t sum = data_out[0] + data_out[1] + data_out[2] + data_out[3];
        if (sum == data_out[4]) {
            return 0; // Success
        }
        
        uart_print("DHT11: Checksum error, retrying...\r\n");
        delay_ms(100); // Wait before retry
				
				//data_out print
				for (int k=0; k<5; k++) {
					sprintf(buf1,"data_out[%d]: %d\r\n", k, data_out[k]);
					uart_print(buf1);
				}
				
				//byte_array calc
				for (int i = 0; i < 5; i++) byte_array[i] = 0;
				for (int i=0; i<40; i++) {
					if (bit_array[i]==1) {
						sprintf(buf1,"one pos:[%d]\r\n", i);
						uart_print(buf1);
					}
					byte_array[i/8] = (byte_array[i/8] << 1) | (bit_array[i] ? 1 : 0);
				}
				
				//bit_array bits->bytes and byte_array print
				for (int k=0; k<5; k++) {
					bit_array[k] = 
						bit_array[k+7] + 
						bit_array[k+6] * 2+
						bit_array[k+5] * 2* 2+
						bit_array[k+4] * 2* 2* 2+
						bit_array[k+3] * 2* 2* 2* 2+
						bit_array[k+2] * 2* 2* 2* 2* 2+
						bit_array[k+1] * 2* 2* 2* 2* 2* 2+
						bit_array[k+0] * 2* 2* 2* 2* 2* 2* 2;
//					data_out[k] = 
//						bit_array[k+7] + 
//						bit_array[k+6] * bit_array[k+6] +
//						bit_array[k+5] * bit_array[k+5] * bit_array[k+5] +
//						bit_array[k+4] * bit_array[k+4] * bit_array[k+4] * bit_array[k+4] +
//						bit_array[k+3] * bit_array[k+3] * bit_array[k+3] * bit_array[k+3] * bit_array[k+3] +
//						bit_array[k+2] * bit_array[k+2] * bit_array[k+2] * bit_array[k+2] * bit_array[k+2] 
//						* bit_array[k+2] +
//						bit_array[k+1] * bit_array[k+1] * bit_array[k+1] * bit_array[k+1] * bit_array[k+1] 
//						* bit_array[k+1] * bit_array[k+1] +
//						bit_array[k+0] * bit_array[k+0] * bit_array[k+0] * bit_array[k+0] * bit_array[k+0] 
//						* bit_array[k+0] * bit_array[k+0] * bit_array[k+0] +
					sprintf(buf1,"byte_array[%d]: %d\r\n", k, byte_array[k]);
					uart_print(buf1);
					delay_ms(50);
				}
    }

    return -1; // All retries failed
}
