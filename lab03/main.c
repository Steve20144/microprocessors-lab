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
#define LED_PIN								PA_2

volatile int counter_button = 0;
volatile char buff2[64];
typedef enum { mode_a, mode_b} mode_profile;
volatile mode_profile profile = mode_a;
volatile int refresh_rate = 1;
volatile int aem = 12345;
volatile uint8_t* results;
volatile int temperature_cnt = 0;
volatile int humidity_cnt = 0;
volatile bool status_called = 0;

// UART RX buffer and queue
volatile char buff[UART_RX_BUFFER_SIZE];
uint32_t buff_index;
Queue rx_queue;

// Function prototypes
void button_isr(int sources);	// for the touch sensor interrupt
void uart_rx_isr(uint8_t rx);	// for the uart
void timer_isr(void);					// for the timer interrupt
void status_report(void);			// for the status report
void dht_print(void);					// prints stuff from dht
void alert_mode(void);				// actions when mode_b is on

int main(void){
	uint8_t rx_char = 0;
	//static uint8_t* results;
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

	// --- Initialize LED ---
	gpio_set_mode(LED_PIN, Output);
	gpio_set(LED_PIN, 0);
	
	// --- Initialize DHT11 ---
	gpio_set_mode(PA_0, Output);
	gpio_set(PA_0, 1);
	sprintf(buff,"\r\ngpio: %d\r\n", gpio_get(PA_0));
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
		switch (counter_button%3) {
			//switch to mode B
			case 0:
				profile = mode_b;
				uart_print("switch to mode B\r\n");
				break;
			//switch to mode A
			case 1:
				profile = mode_a;
				uart_print("switch to mode A\r\n");
				break;
			//calculate new refresh rate
			case 2:
				refresh_rate=( ( (aem%100-aem%10)/10 ) + aem%10 );
				if (refresh_rate<2) {
					refresh_rate = 2;	//minimum rate
				}
				else if (refresh_rate>10) {
					refresh_rate = 10;	//maximum rate
				}
				sprintf(buff2, "New refresh rate from AEM: %d\r\n", refresh_rate);
				uart_print(buff2);
				break;
		}
//		if (counter_button == 3) {
//			counter_button = 0;
//		}
//		else {
		counter_button++;	//increment button counter
//		}
	}
	gpio_set(P_DBG_ISR, 0);
}

void timer_isr(void) {
	//todo
}

void dht_print(void) {
	results = dht11_poll();
	sprintf(buff, "Humidity: %d.%d%%RH\r\nTemperature: %d.%dC\r\n", 
		results[0], results[1], results[2], results[3]);
	uart_print(buff);
	free(results);
}

void alert_mode(void) {
	if (profile==mode_b) {
		//Temperature check
		if (results[2]>25) {
			temperature_cnt=5;
		}
		else {
			if (temperature_cnt>0) {
				temperature_cnt--;
			}
			else {
				temperature_cnt=0;
			}
		}
		//Humidity check
		if (results[0]>60) {
			humidity_cnt=5;
		}
		else {
			if (humidity_cnt>0) {
				humidity_cnt--;
			}
			else {
				humidity_cnt=0;
			}
		}
		//Blink led if temperature or humidity out of limits
		if ( (temperature_cnt>0) || (humidity_cnt>0) ) {
			gpio_toggle(LED_PIN);	//might not turn it off the last time
		}
		else if ( (temperature_cnt==0) && (humidity_cnt==0) ) {
			gpio_set(LED_PIN, 0);	//turns it off the last time
		}
	}
}

void status_report(void) {
	int profile_switches=0;
	profile_switches = ( ((counter_button/3) * 2) + (counter_button%3) );
	if (status_called==1) {
		//print mode
		if (profile==mode_a) {
			uart_print("Mode A/r/n");
		}
		else {
			uart_print("Mode B/r/n");
		}
		//print dht
		dht_print();
		//profile switches
		
		sprintf(buff2, "Profile switches: %d", profile_switches);
		uart_print(buff2);
	}
}

// Push valid chars into rx_queue
void uart_rx_isr(uint8_t rx) {
	if ((rx >= '0' && rx <= '9') || rx == '\r' || rx == '\n' || rx == '-' || 1) {	//PASS ALL
		queue_enqueue(&rx_queue, rx);
	}
}
