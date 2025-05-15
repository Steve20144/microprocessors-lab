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
#define UART_RX_BUFFER_SIZE 128 //read buffer length

Queue rx_queue; // Queue for storing received characters

// Interrupt Service Routine for UART receive
void uart_rx_isr(uint8_t rx) {
	// Check if the received character is a printable ASCII character
	if (rx >= 0x0 && rx <= 0x7F ) {
		// Store the received character
		queue_enqueue(&rx_queue, rx);
	}
}

volatile char buff[UART_RX_BUFFER_SIZE]; // The UART read string will be stored here
volatile uint32_t buff_index = 0;
volatile uint32_t string_index = 0;
typedef enum { STATE_IDLE, STATE_RUNNING, STATE_LOCKED, STATE_LOOPED } run_state_t;
volatile run_state_t state = STATE_IDLE;

int main() {

	// Variables to help with UART read
	uint8_t rx_char = 0;
	// char buff[UART_RX_BUFFER_SIZE]; // The UART read string will be stored here
	// uint32_t buff_index;

	// Initialize the receive queue and UART
	queue_init(&rx_queue, 128);
	uart_init(BAUD_RATE);
	uart_set_rx_callback(uart_rx_isr); // Set the UART receive callback function
	uart_enable(); // Enable UART module

	__enable_irq(); // Enable interrupts

	uart_print("\r\n");// Print newline

	timer_init(500); // 500ms interval
	timer_set_callback(timer_isr);

	while(1) {

		// Prompt the user to enter their full name
		uart_print("Enter the number sequence: ");
		buff_index = 0; // Reset buffer index

		do {
			// Wait until a character is received in the queue
			while (!queue_dequeue(&rx_queue, &rx_char))
				__WFI(); // Wait for Interrupt

				//CAUTION
				//possibly causes issue with button press(?)
				timer_disable();	//stops timer when recieving input from terminal
				state = STATE_IDLE;	//sets state to IDLE

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

		string_index = 0;	//sets index back to the start of the string before running
		timer_enable();
	}
}

void timer_isr(void) {
	if (state == STATE_LOOPED) {				//case: continious loop
		if (buff[string_index] != '\0') {		//as long as it's not the end of the stirng
			process_digit(buff[string_index]);	//sends char to process_digit
			string_index++;
		}
		else if(buff[string_index] == '\0') {	//reached end of string
			string_index=0;						//sets index to 0 and thus loops back from start
		}
	}
	else if (state = STATE_RUNNING) {			//case: single loop
		if (buff[string_index] != '\0') {		//as long as it's not the end of the stirng
			process_digit(buff[string_index]);	//sends char to process_digit
			string_index++;
		}
		else if(buff[string_index] == '\0') {	//reached end of string
			state = STATE_IDLE;					//changes state to IDLE and disables itself
			timer_disable();
		}
	}
	else {										//case: has exited timer loop
		timer_disable();						//disables itself, probably unecessary
	}
}

void process_digit(char digit) {

	if (digit >= '0' && digit <= '9') {
		int num = digit - '0';			//converts char to int
		if (num % 2 == 0) {
			uart_print("Digit ");
			uart_print(&digit);			//should it be &?
			uart_print(" -> Blink LED\r\n");
			if (state != STATE_LOCKED) {
				//gpio_toggle(LED_PIN);
				delay_ms(200); 			// 200ms delay
				//gpio_toggle(LED_PIN);
				delay_ms(200); 			// 200ms delay
			}
		} else {
			uart_print("Digit ");
			uart_print(&digit);
			uart_print(" -> Toggle LED\r\n");
			if (state != STATE_LOCKED) {
				//gpio_toggle(LED_PIN);
			}
		}
	}
}

void gpio_set_mode(uint8_t pin, uint8_t mode) {
	// Set the mode of the GPIO pin (Input, Output, Pullup, etc.)
}

void gpio_set(uint8_t pin, uint8_t value) {
	// Set the value of the GPIO pin (0 or 1)
}

void gpio_toggle(uint8_t pin) {
	// Toggle the value of the GPIO pin
}

bool gpio_get(uint8_t pin) {
	// Get the value of the GPIO pin
	return false; // Placeholder
}

void gpio_set_interrupt(uint8_t pin, void (*isr)(int)) {
	// Set up an interrupt for the GPIO pin
}
