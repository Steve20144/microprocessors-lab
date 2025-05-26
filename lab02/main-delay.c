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

// State machine for LED processing
typedef enum { STATE_IDLE, STATE_RUNNING} run_state_t;
volatile run_state_t state = STATE_IDLE;

volatile bool looped_state = false;	// for if the processing will loop
volatile bool button_state = false;	// for if the button is pressed
volatile int32_t counter100 = 0;    // beat counter (0 to 5)
volatile int32_t timer_pos = 0;     // next character position in buff[] to process
volatile int32_t ledstate = 0;		// for blink funtion action
volatile int counter_button = 0;				// Button press counter
volatile char rx = '0';				// the currently processed character
volatile char buf[64];				// buffer to print with uart_print

// UART RX buffer and queue
volatile char buff[UART_RX_BUFFER_SIZE];
uint32_t buff_index;
Queue rx_queue;

// Function prototypes
void button_isr(int sources);	// for the button interrupt
void uart_rx_isr(uint8_t rx);	// for the uart
void timer_isr(void);			// for the timer interrupt
void digitProcess(void);		// for the digits processing
void blink(void);				// for the led blinking

int main(void) {
	uint8_t rx_char = 0;

	// --- Initialize GPIO button ---
	gpio_set_mode(P_SW, PullDown);           // on-board switch as pull-down
	gpio_set_trigger(P_SW, Rising);          // trigger on rising edge
	gpio_set_callback(P_SW, button_isr);

	// --- Initialize UART with RX queue ---
	queue_init(&rx_queue, UART_RX_BUFFER_SIZE);
	uart_init(BAUD_RATE);
	uart_set_rx_callback(uart_rx_isr);
	uart_enable();

	// --- Initialize LEDs ---
	leds_init();

	// --- Initialize 0.1s timer (100,000 µs) ---
	//(CLK_FREQ/10)=1s
	timer_init(CLK_FREQ/100);												//clock_freq=160khz??
	timer_disable();                         // start disabled
	timer_set_callback(timer_isr);




	// --- Set interrupt priorities ---

	NVIC_SetPriorityGrouping(2);

	NVIC_SetPriority(TIM2_IRQn, 			2);		//Timer priority
	NVIC_SetPriority(USART2_IRQn,     1);    // UART priority
	NVIC_SetPriority(EXTI15_10_IRQn,  3);    // Button highest

	__enable_irq();

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

		// Begin processing digits in timer ISR
		state = STATE_RUNNING;	// sets state from idle to running
		//check if the processing will be looped
		if (buff[buff_index - 2] == '-') {
			looped_state = true;	//loop
		}
		else {
			looped_state = false;	//once
		}

		timer_enable();	//start timer isr
	}
}



// Push valid chars into rx_queue
void uart_rx_isr(uint8_t rx) {
	if ((rx >= '0' && rx <= '9') || rx == '\r' || rx == '\n' || rx == '-') {
		queue_enqueue(&rx_queue, rx);
	}
}



// Button press ISR: toggle lock state on each press
void button_isr(int sources) {
	gpio_set(P_DBG_ISR, 1);
	if ((sources << GET_PIN_INDEX(P_SW)) & (1 << GET_PIN_INDEX(P_SW))) {
		//
		if(!(counter_button%2)){		//if pressed at odd times, then lock
			button_state = true;
			sprintf(buf,
					"Interrupt: Button pressed. LED Locked. Count = %d\r\n",
		   counter_button);
			uart_print(buf);

		}
		else {				//if pressed at even times, then unlock
			button_state = false;
			sprintf(buf,
					"Interrupt: Button pressed. LED Unlocked. Count = %d\r\n",
		   counter_button);
			uart_print(buf);
		}
		counter_button++;	//increment button counter
	}
	gpio_set(P_DBG_ISR, 0);
}



// Timer ISR: handle one digit per 5 ticks
void timer_isr(void) {
	printf("Counter100: %d\rn", counter100);	//debug
	if (state != STATE_IDLE) {
		digitProcess();	//process digits if not idle
	}

	if ((counter100++) > 3) {
		counter100 = 0;	//reset the timer counter after 5 beats (0, 1, 2, 3, 4)
	}
}


// Processing digits
void digitProcess(void) {
	switch (counter100) {
		case 0:
			// 1st beat (0ms)
			// 1) if we've run out of characters, stop if state==running, loop if state==looped
			if (looped_state == false && buff[timer_pos] == '\0') {
				timer_pos = 0;                     // reset for next run
				timer_disable();
				uart_print("End of sequence. Waiting for new number...\r\nEnter a number: ");	//does this appear?
				state = STATE_IDLE;
				counter100=0;
				return;
			}
			else if (looped_state == true && buff[timer_pos] == '\0') {
				timer_pos = 0;                     // reset for next run
			}

			// 2) pull next digit directly from buff[]
			rx = buff[timer_pos++];

			// 3) sanitize input and convert ASCII to integer
			if (rx >= '0' && rx <= '9'){
				int d = rx - '0';
			// 4) blinking and output
				if ((d & 1) == 0) {							//faster implementation of d%2==0
					// even digit: blink 200ms on/off
					ledstate = 0;
					blink();
				} else {
					// odd digit: toggle and hold
					ledstate = 2;
					blink();
				}
			}
			break;
		case 2:
			// 3rd beat (200ms = 0ms + 100ms + 100ms)
			if (ledstate == 0) {	//activates only if the current digit is even
				ledstate = 1;		//this will turn off the led
				blink();
			}
			break;
		default:
			//skip the rest of the beats (100ms, 300ms, 400ms and 500ms)
			break;
	}
}

void blink(void) {
	switch (ledstate) {
		case 0:
			// even digit: start blink 200ms on/off
			// even digit: turn on
			if (button_state == false) {	//do unless button is pressed/leds are locked
				gpio_set(P_LED_R, 1);
			}
			sprintf(buf, "\nDigit %c (even): LED ON\r\n", rx);
			uart_print(buf);
			break;
		case 1:
			// even digit: turn off
			if (button_state == false) {	//do unless button is pressed/leds are locked
				gpio_set(P_LED_R, 0);
			}
			sprintf(buf, "\nDigit %c (even): LED OFF\r\n", rx);
			uart_print(buf);
			break;
		case 2:
			// odd digit: toggle and hold
			if (button_state == false) {	//do unless button is pressed/leds are locked
				gpio_toggle(P_LED_R);
			}
			sprintf(buf, "\nDigit %c (odd): LED toggled\r\n", rx);
			uart_print(buf);
			break;
		default:
			// exeption, this shouldn't happen
			uart_print("\r\nSWITCH CASE FAILED\r\n");
			printf("\r\nSWITCH CASE FAILED\r\n");
			break;
	}
}
