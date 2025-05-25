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
typedef enum { STATE_IDLE, STATE_RUNNING, STATE_LOCKED } run_state_t;
volatile run_state_t state = STATE_IDLE;

volatile bool looped_state = false;
volatile bool delay200_state = false;
volatile bool blink_state = false;
volatile bool button_state = false;
volatile uint64_t counter100 = 0;    //((( (max uint64 = 18 446 744 073 709 551 615) × 0,1ms)/60)/60)/24)/365 ˜ 58 494 241 736 years > 58 billion years
volatile uint64_t timer_counter500 = 0;
volatile uint64_t timer_counter200 = 0;
volatile int ledstate = 0;
volatile char rx2 = '0';

// Button press counter
static int count = 0;

// UART RX buffer and queue
volatile char buff[UART_RX_BUFFER_SIZE];
uint32_t buff_index;
Queue rx_queue;

// Function prototypes
void button_isr(int sources);
void uart_rx_isr(uint8_t rx);
void timer_isr(void);
void currentTime_isr(void);
void delay200(void);
void digitProcess(void);
void delay_isr(void);
void blink(void);

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
	timer_init(400000);												//clock_freq=160khz
	timer_disable();                         // start disabled
	timer_set_callback(timer_isr);




	// --- Set interrupt priorities ---

	NVIC_SetPriorityGrouping(2);

	NVIC_SetPriority(TIM2_IRQn, 			2);		//Timer priority
	NVIC_SetPriority(USART2_IRQn,     1);    // UART priority
	NVIC_SetPriority(EXTI15_10_IRQn,  3);    // Button highest

	__enable_irq();

	uart_print("\r\n");

	// --- Main loop: prompt, read line, then start blinking ---
	while (1) {


		uart_print("Enter a number:");
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

		if(state != STATE_LOCKED) state = STATE_RUNNING;
		if (buff[buff_index - 2] == '-') {
			looped_state = true;	//loop
		}
		else {
			looped_state = false;	//once
		}

		timer_counter500 = counter100; //holds the time when function is called
		timer_enable();
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
		if(!(count%2)){
			//state = STATE_LOCKED;
			button_state = true;


			char buf[64];
			sprintf(buf,
					"Interrupt: Button pressed. LED Locked. Count = %d\r\n",
		   count);
			uart_print(buf);

		}
		else {
			//state = STATE_RUNNING;
			button_state = false;
			char buf[64];
			sprintf(buf,
					"Interrupt: Button pressed. LED Unlocked. Count = %d\r\n",
		   count);
			uart_print(buf);
		}
		count++;
	}
	gpio_set(P_DBG_ISR, 0);
}




// Timer ISR: handle one digit per tick
void timer_isr(void) {
	printf("\ncounter100: %lld\tcounter500: %lld",counter100,timer_counter500);
	//activates every 5 beats
	if ( delay200_state == false && (counter100 - 5 - timer_counter500) == 0 ) {
		timer_counter500 = counter100; //holds the time when function is called
		printf("\ncounter100': %lld\tcounter500': %lld",counter100,timer_counter500);
		if (state != STATE_IDLE) {
			digitProcess();
		}
	}
	else if ( delay200_state == true ) {
		delay200();
	}
	counter100++;
}

void delay_isr(void) {
	delay200();
	counter100++;
}

//delay200
void delay200(void) {
	printf("\ncounter100: %lld\tcounter200: %lld",counter100,timer_counter200);
	if ( (counter100 - 2 - timer_counter200) == 0 ) {
		// if (blink_state == false) {
		printf("\ncounter100': %lld\tcounter200': %lld",counter100,timer_counter200);
		//timer_counter200 = counter100; //holds the time when function is called
		delay200_state = false;
		timer_counter200 = 0;
		timer_counter500 = 1 + counter100;

		blink();
		// }
		// else if (blink_state == true) {
		// 	blink_state = false;
		// 	//turns off led and goes back to delay mode
		// 	gpio_set(P_LED_R, 0);
		// 	sprintf(buf, "\nDigit %c (even): LED OFF\r\n", rx);
		// 	uart_print(buf);
		// 	timer_counter200 = counter100;
		// 	delay200_state = true;
		// 	delay200();
		// 	//delay_ms(200);
		// }
	}
}

void digitProcess(void) {
	static uint32_t timer_pos = 0;        // next character in buff[] to process
	char buf[64];

	// buff_index was set in main as (characters read + 1 for '\r').
	// After null-termination in main, the valid digits are in buff[0]…buff[buff_index-2].
	//uint32_t buff_len = buff_index - 1;		//how about we do that in main

	// 1) if we've run out of characters, stop if state==running, loop if state==looped
	if (looped_state == false && buff[timer_pos] == '\0') {
		timer_pos = 0;                     // reset for next run
		timer_disable();
		uart_print("End of sequence. Waiting for new number...\r\n");	//does this appear?
		state = STATE_IDLE;
		return;
	}
	else if (looped_state == true && buff[timer_pos] == '\0') {


		timer_pos = 0;                     // reset for next run
	}

	// 2) pull next digit directly from buff[]
	char rx = buff[timer_pos++];
	// 3) convert ASCII to integer
	if (rx >= '0' && rx <= '9'){
		//

		int d = rx - '0';

		// 4) if button isn't pressed
		if (state == STATE_RUNNING) {
			if ((d & 1) == 0) {							//faster implementation of d%2==0
				// even digit: blink 200ms on/off
				ledstate = 0;
				rx2=rx;
				blink();
				// gpio_set(P_LED_R, 1);
				// sprintf(buf, "\nDigit %c (even): LED ON\r\n", rx);
				// uart_print(buf);
				// timer_counter200 = counter100;
				// delay200_state = true;
				// timer_disable();
				// timer_set_callback(delay_isr);
				// timer_enable();
				// while (delay200_state==true) {
				// 	__WFI();
				// }
				// timer_disable();
				// timer_set_callback(timer_isr);
				// timer_enable();
				// //delay200();
				// //delay_ms(200);
    //
				// gpio_set(P_LED_R, 0);
				// sprintf(buf, "\nDigit %c (even): LED OFF\r\n", rx);
				// uart_print(buf);
				// timer_counter200 = counter100;
				// delay200_state = true;
				// delay200();
				// //delay_ms(200);
			} else {
				ledstate = 4;
				rx2=rx;
				blink();
				// odd digit: toggle and hold
//				gpio_toggle(P_LED_R);
//				sprintf(buf, "\nDigit %c (odd): LED toggled\r\n", rx);
//				uart_print(buf);
			}
		}
		if (state == STATE_LOCKED){
			if ((d & 1) == 0) {							//faster implementation of d%2==0
				// even digit: blink 200ms on/off
				// gpio_set(P_LED_R, 1);
				sprintf(buf, "\nDigit %c (even): LED ON\r\n", rx);
				uart_print(buf);
				timer_counter200 = counter100;
				delay200_state = true;
				delay200();
				//delay_ms(200);

				// gpio_set(P_LED_R, 0);
				sprintf(buf, "\nDigit %c (even): LED OFF\r\n", rx);
				uart_print(buf);
				timer_counter200 = counter100;
				delay200_state = true;
				delay200();
				//delay_ms(200);
			} else {
				// odd digit: toggle and hold
				// gpio_toggle(P_LED_R);
				sprintf(buf, "\nDigit %c (odd): LED toggled\r\n", rx);
				uart_print(buf);
			}
		}
	}
}

void blink(void) {
	char buf2[64];
	// even digit: blink 200ms on/off
	// even digit: turn on
	if (ledstate == 0) {
		if (button_state == false) {
			gpio_set(P_LED_R, 1);
		}
		sprintf(buf2, "\nDigit %c (even): LED ON\r\n", rx2);
		uart_print(buf2);

		timer_counter200 = counter100;
		delay200_state = true;
		timer_disable();
		timer_set_callback(delay_isr);
		timer_enable();
		ledstate = 1;
	}
	// even digit: turn off
	else if (ledstate == 1) {
		if (button_state == false) {
		gpio_set(P_LED_R, 0);
		}
		sprintf(buf2, "\nDigit %c (even): LED OFF\r\n", rx2);
		uart_print(buf2);
		timer_counter200 = counter100;
		delay200_state = true;
		ledstate = 2;
	}
	// even digit: disable delay
	else if (ledstate == 2) {
		ledstate = 0;
		timer_counter200 = counter100;
		delay200_state = false;
		timer_counter500 = 1 + counter100;
		timer_disable();
		timer_set_callback(timer_isr);
		timer_enable();
	}
	// odd digit: toggle and hold
	else if (ledstate == 4) {
		ledstate = 0;
		if (button_state == false) {
		gpio_toggle(P_LED_R);
		}
		sprintf(buf2, "\nDigit %c (odd): LED toggled\r\n", rx2);
		uart_print(buf2);
	}
}
