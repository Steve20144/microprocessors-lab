//#include "gpio.h"
//#include "platform.h"
//#include "stm32f4xx.h"
//#include "uart.h"
#include "dht11.h"
//#include <stdio.h>
//#include "delay.h"
#include <stdlib.h>
//#include <stdint.h>
#include "stm32f4xx.h"    // CMSIS/LL definitions
#include "platform.h"     // Board-specific base addresses, IRQs, pin-mux macros
#include <stdio.h>        // sprintf, printf
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "uart.h"         // UART driver
#include "timer.h"        // Timer driver
#include "leds.h"         // LED driver
#include "queue.h"        // Simple FIFO queue
#include "gpio.h"         // GPIO driver
#include "delay.h"        // Busy-wait delay functions

#define BAUD_RATE             115200
#define UART_RX_BUFFER_SIZE   128
#define MAX_DIGITS            128

volatile int counter_button = 0;
volatile char buff2[64];

// UART RX buffer and queue
volatile char buff[UART_RX_BUFFER_SIZE];
uint32_t buff_index;
Queue rx_queue;

// Function prototypes
void button_isr(int sources);	// for the touch sensor interrupt
void uart_rx_isr(uint8_t rx);	// for the uart
void timer_isr(void);					// for the timer interrupt

int main(void){
	uint8_t rx_char = 0;
	static uint8_t* results;
	char buff[64];
	
	// --- Initialize GPIO touch-sensor ---
	gpio_set_mode(PA_1, PullDown);           // on-board switch as pull-down
	gpio_set_trigger(PA_1, Rising);          // trigger on rising edge
	gpio_set_callback(PA_1, button_isr);
	gpio_set_mode(PA_1, Input);		//it works this way, maybe I should remove the Pulldown mode
	
	// --- Initialize UART with RX queue ---
	queue_init(&rx_queue, UART_RX_BUFFER_SIZE);
	uart_init(BAUD_RATE);
	uart_set_rx_callback(uart_rx_isr);
	uart_enable();

	// --- Initialize LEDs ---
	leds_init();
	
	// --- Initialize DHT11 ---
	gpio_set_mode(PA_0, Output);
	gpio_set(PA_0, 1);
	sprintf(buff,"gpio: %d\r\n", gpio_get(PA_0));
	uart_print(buff);
	
	// --- Initialize 0.1s timer (100,000 µs) ---
	//(CLK_FREQ/10)=1s
	timer_init(CLK_FREQ/10);		//clock_freq=160khz??
	timer_disable();				// start disabled
	timer_set_callback(timer_isr);
	
	// --- Set interrupt priorities ---

	NVIC_SetPriorityGrouping(2);

	NVIC_SetPriority(TIM2_IRQn,			2);	// Timer priority
	NVIC_SetPriority(USART2_IRQn,		1);	// UART priority
	NVIC_SetPriority(EXTI15_10_IRQn,	3);	// Button highest
	__enable_irq();
	
	
	//DHT11 test
	results = dht11_poll();
	sprintf(buff, "Humidity: %d.%d%%RH\r\nTemperature: %d.%dC\r\n", 
		results[0], results[1], results[2], results[3]);
	uart_print(buff);
	free(results);
	
	//touch-sensor test
	while(0){	//turned off for now
		//sprintf(buff, "Touch sensor: %d\r\n", gpio_get(PA_1));
		//uart_print(buff);
		delay_ms(1000);
		//break;
	}
	
	// UART input #START
	uart_print("\r\nEnter a number: ");

	// --- Main loop: prompt, read line, then start blinking ---
	while (1) {
		
		//uart_print("Enter a number:");
		buff_index = 0;
		// Read chars into buff until '\r' or full
		do {
			
			// Wait for next byte in queue
			while (!queue_dequeue(&rx_queue, &rx_char)) {
				__WFI();
			}

			if (rx_char == 0x7F) {  // backspace
				if (buff_index > 0) {
					buff_index--;
					uart_tx(rx_char);  // echo backspace
				}
			} else {
				buff[buff_index++] = (char)rx_char;
				uart_tx(rx_char);    // echo
			}
		} while (rx_char != '\r' && buff_index < UART_RX_BUFFER_SIZE);

		// Null-terminate string (overwrite '\r')
		if (buff_index > 0) {
			buff[buff_index - 1] = '\0';
		}
		uart_print("\r\n");

		// Check for overflow
		if (buff_index >= UART_RX_BUFFER_SIZE) {
			uart_print("Stop trying to overflow my buffer! I resent that!\r\n");
			continue;
		}
	// UART input #END
	
	}
	timer_enable();	//start timer isr
}

// Button press ISR: toggle lock state on each press
void button_isr(int sources) {
	gpio_set(P_DBG_ISR, 1);
	if ((sources << GET_PIN_INDEX(PA_1)) & (1 << GET_PIN_INDEX(PA_1))) {
		switch (counter_button) {
			//switch to mode B
			case 0:
				uart_print("switch to mode B\r\n");
				break;
			//switch to mode A
			case 1:
				uart_print("switch to mode A\r\n");
				break;
			//calculate new refresh rate
			case 2:
				uart_print("calculate new refresh rate\r\n");
				break;
		}
		if (counter_button == 3) {
			counter_button = 0;
		}
		else {
			counter_button++;	//increment button counter
		}
	}
	gpio_set(P_DBG_ISR, 0);
}

void timer_isr(void) {
	//todo
}

// Push valid chars into rx_queue
void uart_rx_isr(uint8_t rx) {
	if ((rx >= '0' && rx <= '9') || rx == '\r' || rx == '\n' || rx == '-' || 1) {	//PASS ALL
		queue_enqueue(&rx_queue, rx);
	}
}
