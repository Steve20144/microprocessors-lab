#include "gpio.h"
#include "platform.h"
#include "stm32f4xx.h"
#include "uart.h"
#include "dht11.h"
#include <stdio.h>
#include "delay.h"
#include <stdlib.h>
#include <stdint.h>

void button_isr(int sources);	// for the button interrupt
volatile int counter_button = 0;
volatile char buff2[64];

//volatile uint8_t* results;
int main(void){
	static uint8_t* results;
	//results = malloc(sizeof(uint8_t) * 5);
	char buff[64];
	
	uart_init(115200);
	uart_enable();
	gpio_set_mode(PA_0, Output);
	gpio_set(PA_0, 1);
	sprintf(buff,"gpio: %d\r\n", gpio_get(PA_0));
	uart_print(buff);
	
	__enable_irq();
	
	dht11_poll(results);
		
		sprintf(buff, "Humidity: %d\r\nTemperature: %d\r\n", results[0], results[2]);
		uart_print(buff);
	
	gpio_set_mode(PA_1, PullDown);           // on-board switch as pull-down
	gpio_set_trigger(PA_1, Rising);          // trigger on rising edge
	gpio_set_callback(PA_1, button_isr);
	
	gpio_set_mode(PA_1, Input);
	while(0){	//turned off for now
		//sprintf(buff, "Touch sensor: %d\r\n", gpio_get(PA_1));
		//uart_print(buff);
		delay_ms(500);
		//break;
		
	}
	free(results);
}

// Button press ISR: toggle lock state on each press
void button_isr(int sources) {
	gpio_set(P_DBG_ISR, 1);
	if ((sources << GET_PIN_INDEX(PA_1)) & (1 << GET_PIN_INDEX(PA_1))) {
		//
		if(!(counter_button%2)){		//if pressed at odd times, then lock
			//button_state = true;
			sprintf(buff2,
					"Interrupt: Button pressed. LED Locked. Count = %d\r\n",
		   counter_button);
			uart_print(buff2);
		}
		else {				//if pressed at even times, then unlock
			//button_state = false;
			sprintf(buff2,
					"Interrupt: Button pressed. LED Unlocked. Count = %d\r\n",
		   counter_button);
			uart_print(buff2);
		}
		counter_button++;	//increment button counter
	}
	gpio_set(P_DBG_ISR, 0);
}
