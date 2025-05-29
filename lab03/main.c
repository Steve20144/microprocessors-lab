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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "uart.h"         // UART driver
#include "timer.h"        // Timer driver
#include "leds.h"         // LED driver
#include "queue.h"        // Simple FIFO queue
#include "gpio.h"         // GPIO driver
#include "delay.h"        // Busy-wait delay functions
#include "customtimers.h"


#define BAUD_RATE             115200
#define UART_RX_BUFFER_SIZE   128
#define MAX_DIGITS            128
#define LED_PIN               PA_2
#define PINCODE 							123

volatile int counter_button = 0;
volatile char buff2[64];
typedef enum { mode_a, mode_b } mode_profile;
volatile mode_profile profile = mode_a;
volatile int refresh_rate = 1;
volatile int aem = 12345;
volatile uint8_t* results;
volatile int temperature_cnt = 0;
volatile int humidity_cnt = 0;
volatile bool status_called = 0;
volatile int timer = 1000;

// UART RX buffer and queue
volatile char buff[UART_RX_BUFFER_SIZE];
uint32_t buff_index;
Queue rx_queue;

// Function prototypes
void button_isr(int sources);    // for the touch sensor interrupt
void uart_rx_isr(uint8_t rx);    // for the uart
void timer_isr(void);            // for the timer interrupt
void status_report(void);        // for the status report
void dht_print(void);            // prints stuff from dht
void alert_mode(void);           // actions when mode_b is on

static void sample_dht11(void);
static void board_init(void);
static void application_loop(void);
static void system_login(void);
static void increment_sampling(void);

int main(void) {
    board_init();          // one-time setup
		system_login();					//Password and AEM logic
    application_loop();    // runs until you want to exit
    NVIC_SystemReset();    // reset if loop ever returns
    while (1) { /* should never get here */ }
}

// ------------------------------------------------------------------------------------------------
//DHT11 Sample and print
static void sample_dht11(void){
	// --- One-shot DHT11 test read ---
    results = dht11_poll();
    {
        char msg[64];
        sprintf(msg,
                "Humidity: %d.%d%%RH\r\nTemperature: %d.%dC\r\n",
                results[0], results[1],
                results[2], results[3]);
        uart_print(msg);
    }
    free(results);
	}


// Initialize GPIO, UART, timer, DHT11, NVIC, etc.
static void board_init(void) {
    // --- Initialize GPIO touch-sensor on PA1 ---
    gpio_set_mode(PA_1, PullDown);
    gpio_set_trigger(PA_1, Rising);
    gpio_set_callback(PA_1, button_isr);
    gpio_set_mode(PA_1, Input);

    // --- Initialize UART with RX queue ---
    queue_init(&rx_queue, UART_RX_BUFFER_SIZE);
    uart_init(BAUD_RATE);
    uart_set_rx_callback(uart_rx_isr);
    uart_enable();

    // --- (LED_PIN setup, disabled if not working) ---
    // gpio_set_mode(LED_PIN, Output);
    // gpio_set(LED_PIN, 0);

    // --- Initialize DHT11 pin and debug print ---
    gpio_set_mode(PA_0, Output);
    gpio_set(PA_0, 1);
    {
        char tmp[64];
        sprintf(tmp, "gpio PA0: %d\r\n", gpio_get(PA_0));
        uart_print(tmp);
    }

    // --- Initialize 0.1s timer ---
    timer_init(timer);
    timer_disable();
    timer_set_callback(timer_isr);

    // --- Set interrupt priorities and enable IRQs ---
    NVIC_SetPriorityGrouping(2);
    NVIC_SetPriority(TIM2_IRQn,       2);
    NVIC_SetPriority(USART2_IRQn,     1);
    NVIC_SetPriority(EXTI15_10_IRQn,  3);
    __enable_irq();

    

    
}

// ------------------------------------------------------------------------------------------------
//Login Function: Enter password and AEM

static void system_login(void) {
		//Disable the button interrupt while we're in the login loop
		NVIC_DisableIRQ(EXTI0_IRQn);
    uint8_t rx_char = 0;
	uint32_t idx =0;
	
		//PIN ENTRY
		while (1) {
        uart_print("Enter PIN: ");
        idx = 0;

        // collect characters until CR or full
        do {
            while (!queue_dequeue(&rx_queue, &rx_char)) {
                __WFI();
            }
            if (rx_char == 0x7F && idx > 0) {
                // backspace
                idx--;
                uart_tx(rx_char);
            } else if (rx_char != 0x7F) {
                buff[idx++] = (char)rx_char;
                uart_tx(rx_char);
            }
        } while (rx_char != '\r' && idx < UART_RX_BUFFER_SIZE);

        // overwrite the '\r' with '\0'
        if (idx > 0) {
            buff[idx - 1] = '\0';
        }
        uart_print("\r\n");

        // overflow?
        if (idx >= UART_RX_BUFFER_SIZE) {
            uart_print("Stop trying to overflow my buffer! I resent that!\r\n");
            continue;
        }

        // simple numeric compare
        if (atoi(buff) == PINCODE) {
            NVIC_EnableIRQ(EXTI0_IRQn);
            break;
        }
        uart_print("Error: Wrong Passcode\r\n");
    }

		
		 // At this point, PIN was correct and you've broken out
    uart_print("PIN accepted. Unlocking...\r\n");
				
			
			
		
	
}



// Main loop: read UART line, then enable timer for periodic work
static void application_loop(void) {
    while (1) {
				
			
        // --- Present menu ---
        uart_print("\r\n=== Main Menu ===\r\n");
        uart_print("a) Increase Sampling Rate by 1s\r\n");
        uart_print("b) Decrease Sampling Rate by 1s\r\n");
        uart_print("c) Change View\r\n");
        uart_print("d) Print latest sample\r\n");
        uart_print("Enter choice: ");
			
				

        // Read one line of input into buff
        buff_index = 0;
        uint8_t rx_char;
        do {
            // Wait for next received char
            while (!queue_dequeue(&rx_queue, &rx_char)) {
                __WFI();
            }
            // Echo and handle backspace
            if (rx_char == 0x7F && buff_index > 0) {
                buff_index--;
                uart_tx(rx_char);
            } else if (buff_index < UART_RX_BUFFER_SIZE - 1) {
                buff[buff_index++] = (char)rx_char;
                uart_tx(rx_char);
            }
        } while (rx_char != '\r');

        // Null-terminate (overwrite '\r')
        if (buff_index > 0) {
            buff[buff_index - 1] = '\0';
        }
        uart_print("\r\n");

        // Dispatch choice
        switch (buff[0]) {
            case 'a':
            case 'A':
							increment_sampling();
                break;
            case 'b':
            case 'B':
                //option_b();
                break;
            case 'c':
            case 'C':
                //option_c();
                break;
            case 'd':
            case 'D':
                
                break;
            default:
                uart_print("Invalid option, please enter a, b, c, or d.\r\n");
                continue;
        }

       
    }
}

// ------------------------------------------------------------------------------------------------
// Button press ISR: toggle modes and report
void button_isr(int sources) {
    gpio_set(P_DBG_ISR, 1);
    if ((sources << GET_PIN_INDEX(PA_1)) & (1 << GET_PIN_INDEX(PA_1))) {
        switch (counter_button) {
            case 0: uart_print("switch to mode B\r\n"); break;
            case 1: uart_print("switch to mode A\r\n"); break;
            case 2: uart_print("calculate new refresh rate\r\n"); break;
        }
        counter_button = (counter_button + 1) % 4;
    }
    gpio_set(P_DBG_ISR, 0);
}

// Periodic timer ISR
void timer_isr(void) {
    // TODO: periodic activity (e.g., call dht_print, alert_mode)
	sample_dht11();
	
}

// Read and print DHT11 data
void dht_print(void) {
    results = dht11_poll();
    sprintf(buff, "Humidity: %d.%d%%RH\r\nTemperature: %d.%dC\r\n",
            results[0], results[1], results[2], results[3]);
    uart_print(buff);
    free(results);
}

// If in mode_b, check thresholds and blink LED_PIN
void alert_mode(void) {
    if (profile == mode_b) {
        // Temperature check
        if (results[2] > 25) {
            temperature_cnt = 5;
        } else if (temperature_cnt > 0) {
            temperature_cnt--;
        }

        // Humidity check
        if (results[0] > 60) {
            humidity_cnt = 5;
        } else if (humidity_cnt > 0) {
            humidity_cnt--;
        }

        // Blink or turn off LED
        if (temperature_cnt > 0 || humidity_cnt > 0) {
            gpio_toggle(LED_PIN);
        } else {
            gpio_set(LED_PIN, 0);
        }
    }
}

static void increment_sampling(){
	 // only allow up to 10 seconds (i.e. 10000 ms)
            if (timer < 10000) {
                timer += 1000;              // bump by 1 s
                timer_disable();            // reconfigure the hardware timer
                timer_init(timer);
                char msg[64];
                sprintf(msg, "Sampling rate set to %d s\r\n", timer / 1000);
                uart_print(msg);
            } else {
                uart_print("Error: maximum sampling interval is 10 s\r\n");
            }
					}

// Print current mode and counters if requested
void status_report(void) {
    int profile_switches = ((counter_button / 3) * 2) + (counter_button % 3);
    if (status_called) {
        if (profile == mode_a) {
            uart_print("Mode A\r\n");
        } else {
            uart_print("Mode B\r\n");
        }
        sprintf(buff2, "Profile switches: %d\r\n", profile_switches);
        uart_print(buff2);
    }
}

// UART RX ISR: enqueue all received bytes
void uart_rx_isr(uint8_t rx) {
    queue_enqueue(&rx_queue, rx);
}
