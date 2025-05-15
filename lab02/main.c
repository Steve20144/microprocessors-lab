#include "stm32f4xx.h"   // 1) the CMSIS/LL definitions
#include "platform.h"    // 2) your board-specific base addresses, IRQs, pin-mux macros, etc.
#include <stdio.h>       // 3) standard headers (order here is mostly a matter of taste)
#include <stdint.h>
#include <string.h>
#include "uart.h"        // 4) now all of your drivers can safely pull in platform.h symbols
#include "timer.h"       
#include "leds.h"
#include "queue.h"
#include "gpio.h"
#include "delay.h"

#define BAUD_RATE 115200
#define UART_RX_BUFFER_SIZE 128 
#define MAX_DIGITS 128

	Queue rx_queue;					// Queue for storing received characters

	typedef enum { STATE_IDLE, STATE_RUNNING, STATE_LOCKED, STATE_LOOPED } run_state_t;
	volatile run_state_t state = STATE_IDLE;
	static int count = 0;								// Button press counter
	

	volatile char buff[UART_RX_BUFFER_SIZE]; 
	uint32_t buff_index;


	void button_isr(int sources);				//Button press interrupt
	void uart_rx_isr(uint8_t rx);					//UART RX interrupt
	void timer_isr(void);					//0.5s timer tick interrupt




	int main(){
		
		//Variables to help with UART read
		
		uint8_t rx_char = 0;
		
		
		
		queue_init(&rx_queue, 128);
		uart_init(115200);
		uart_set_rx_callback(uart_rx_isr);
		uart_enable();
		
		
		
		timer_init(500);
		timer_disable();
		
		timer_set_callback(timer_isr);
		
		
		
		//Set the priorities
		
		
		
		__enable_irq();
		
		
		uart_print("\r\n");
		
		while(1){
			
			uart_print("Enter a number:");
			buff_index=0;
			
			NVIC_DisableIRQ(TIM2_IRQn);         // timer interrupt off
			NVIC_DisableIRQ(EXTI15_10_IRQn);    // button interrupt off
			
			do{
				while(!queue_dequeue(&rx_queue, &rx_char))
					__WFI();
				
				if (rx_char == 0x7F) { // Handle backspace character
					if (buff_index > 0) {
						buff_index--; // Move buffer index back
						uart_tx(rx_char); // Send backspace character to erase on terminal
					}
				} else {
					// Store and echo the received character back
					buff[buff_index++] = (char)rx_char; // Store character in buffer
					uart_tx(rx_char); // Echo character back to terminal
				}
			} while (rx_char != '\r' && buff_index < UART_RX_BUFFER_SIZE); // Continue until Enter key or buffer full
			
			
			
				NVIC_EnableIRQ(EXTI15_10_IRQn);			// restore Timer & Button
				NVIC_EnableIRQ(TIM2_IRQn);
			
			
			// Replace the last character with null terminator to make it a valid C string
			buff[buff_index - 1] = '\0';
			uart_print("\r\n"); // Print newline
			
			// Check if buffer overflow occurred
			 if (buff_index >= UART_RX_BUFFER_SIZE) {
					uart_print("Stop trying to overflow my buffer! I resent that!\r\n");
			}

					
					if (buff[buff_index - 2] == '-') {
						state = STATE_LOOPED;	//loop
					}
					else {
						state = STATE_RUNNING;	//once
					}
					timer_enable();           // kick off timer_isr() every 0.5s			
		

			
		}
		
	}

	void uart_rx_isr(uint8_t rx) {
    // accept digits or CR (and if your terminal sends LF, maybe '\n' too)
    if ((rx >= '0' && rx <= '9') || rx == '\r' || rx == '\n') {
        queue_enqueue(&rx_queue, rx);
    }
}


void button_isr(int sources){
	// run_state_t state_temp = state;		//stores current state
	
	gpio_set(P_DBG_ISR, 1);
	if((sources << GET_PIN_INDEX(P_SW)) & (1 << GET_PIN_INDEX(P_SW))) count++;					// Code from class resources.
	gpio_set(P_DBG_ISR, 0);
	
	// We need to check if this locks or unlocks the LED
	if(!(count%2)){						//If the count is odd, that means that it locked the LED
		char buf[64];						//Workaround for uart printf
	state = STATE_LOCKED;			// If this is the first time we press the button this time (odd) change the state to locked
		int n = sprintf(buf,
                "Interrupt: Button pressed. LED Locked. Count = %d\r\n",
                count);
		uart_print(buf);

	
	
	}
	else state = STATE_RUNNING;		//If count is not odd (zero or even) unlock.
	
	
}

void timer_isr(void) {
	if ( (state == STATE_RUNNING) || (state == STATE_LOOPED) ) {
		static uint32_t timer_pos = 0;        // next character in buff[] to process
		char buf[64];

		// buff_index was set in main as (characters read + 1 for '\r').
		// After null-termination in main, the valid digits are in buff[0]…buff[buff_index-2].
		uint32_t buff_len = buff_index - 1;		//how about we do that in main

		// 1) if we've run out of characters, stop if state==running, loop if state==looped
		if (state == STATE_RUNNING && timer_pos >= buff_len) {
			timer_pos = 0;                     // reset for next run
			timer_disable();
			uart_print("End of sequence. Waiting for new number...\r\n");	//does this appear?
			state = STATE_IDLE;
			return;
		}
		else if (state == STATE_LOOPED && timer_pos >= buff_len) {
			timer_pos = 0;                     // reset for next run
		}

		// 2) pull next digit directly from buff[]
		char rx = buff[timer_pos++];
		// 3) convert ASCII to integer
		int d = rx - '0'; 

		// 4) do exactly the same LED logic you had before:
			if ((d & 1) == 0) {							//faster implementation of d%2==0
				// even digit: blink 200ms on/off
				gpio_set(P_LED_G, 1);
				sprintf(buf, "\nDigit %c (even): LED ON\r\n", rx);
				uart_print(buf);
				delay_ms(200);

				gpio_set(P_LED_G, 0);
				sprintf(buf, "\nDigit %c (even): LED OFF\r\n", rx);
				uart_print(buf);
				delay_ms(200);
			} else {
				// odd digit: toggle and hold
				gpio_toggle(P_LED_G);
				sprintf(buf, "\nDigit %c (odd): LED toggled\r\n", rx);
				uart_print(buf);
			}
    }

//    // 5) log what actually happened
//    if (state == STATE_RUNNING) {
//        sprintf(buf, "\nDigit %c -> Toggled PA5 (LED) %s\r\n",
//                rx, (d & 1) ? "(odd)" : "(even)"); 
//    } else {
//        sprintf(buf, "\nDigit %c -> Skipped state LOCKED\r\n", rx);
//    }
//    uart_print(buf);
}
