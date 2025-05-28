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
	char buff[64];
	
	uart_init(115200);
	uart_enable();
	gpio_set_mode(PA_0, Output);
	gpio_set(PA_0, 1);
	sprintf(buff,"gpio: %d\r\n", gpio_get(PA_0));
	uart_print(buff);
	
	__enable_irq();
	
	results = dht11_poll();
		
		sprintf(buff, "Humidity: %d.%d%%RH\r\nTemperature: %d.%dC\r\n", 
			results[0], results[1], results[2], results[3]);
		uart_print(buff);
	
	gpio_set_mode(PA_1, PullDown);           // on-board switch as pull-down
	gpio_set_trigger(PA_1, Rising);          // trigger on rising edge
	gpio_set_callback(PA_1, button_isr);
	
	gpio_set_mode(PA_1, Input);
	while(1){	//turned off for now
		//sprintf(buff, "Touch sensor: %d\r\n", gpio_get(PA_1));
		//uart_print(buff);
		delay_ms(1000);
		//break;
		
	}
	//free(results);
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
