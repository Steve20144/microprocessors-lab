#include "dht11.h"
#include <stdlib.h>
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
#include <stdbool.h>


#define BAUD_RATE             115200
#define UART_RX_BUFFER_SIZE   128
#define MAX_DIGITS            128
#define LED_PIN               PA_4
#define PINCODE 							123

typedef enum {
    TEMP_MODE,
    HUM_MODE,
    BOTH_MODE
} display_mode_t;

volatile display_mode_t disp_mode = BOTH_MODE;

typedef enum { mode_a, mode_b } mode_profile;
volatile mode_profile profile = mode_a;
volatile uint64_t ticks =0;
volatile int counter_button = 0;
volatile int refresh_rate = 5;
volatile int aem = 0;
volatile int timer = 840000;
volatile int temperature_cnt = 0;
volatile int humidity_cnt = 0;
volatile int temperature_reset_cnt = 0;
volatile int humidity_reset_cnt = 0;
volatile uint8_t* results;
volatile bool sampling = false;
volatile char buff2[64];

// UART RX buffer and queue
volatile char buff[UART_RX_BUFFER_SIZE];
uint32_t buff_index;
Queue rx_queue;

// Function prototypes
void button_isr(int sources);    // for the touch sensor interrupt
void uart_rx_isr(uint8_t rx);    // for the uart
void timer_isr(void);            // for the timer interrupt
void status_report(void);        // for the status report
void alert_mode(void);           // actions when mode_b is on
void blink_alert(void);

static void sample_dht11(void);
static void board_init(void);
static void application_loop(void);
static void system_login(void);
static void increment_sampling(void);
static void decrement_sampling(void);

int main(void) {
    board_init();          // one-time setup
		system_login();					//Password and AEM logic
    application_loop();    // runs until you want to exit
    while (1) { /* should never get here */ }
}

// ------------------------------------------------------------------------------------------------
//DHT11 Sample and print
static void sample_dht11(void) {
    // --- One-shot DHT11 read ---
    results = dht11_poll();

    char msg[64];
    switch (disp_mode) {
        case TEMP_MODE:
            // Only temperature
            sprintf(msg, "Temperature: %d.%dC\r\n",
                    results[2], results[3]);
            break;
        case HUM_MODE:
            // Only humidity
            sprintf(msg, "Humidity: %d.%d%%RH\r\n",
                    results[0], results[1]);
            break;
        case BOTH_MODE:
        default:
            // Both temperature and humidity
            sprintf(msg,
                    "Humidity: %d.%d%%RH\r\n"
                    "Temperature: %d.%dC\r\n",
                    results[0], results[1],
                    results[2], results[3]);
            break;
    }
    uart_print(msg);

    // Reset counters on wild values
    if (results[2] > 35) {
        temperature_reset_cnt++;
    } else {
        temperature_reset_cnt = 0;
    }
    if (results[0] > 80) {
        humidity_reset_cnt++;
    } else {
        humidity_reset_cnt = 0;
    }
    if (temperature_reset_cnt > 2 || humidity_reset_cnt > 2) {
        free(results);
        NVIC_SystemReset();
    }

    // free(results);  // if you prefer to free here instead of in timer
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
    gpio_set_mode(LED_PIN, Output);
    gpio_set(LED_PIN, 0);
		leds_init();

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
    NVIC_SetPriority(USART2_IRQn,     3);
    NVIC_SetPriority(EXTI15_10_IRQn,  1);
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
		
		//AEM
		while (1) {
		uart_print("Enter AEM: ");
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
		
		break;
		}
		aem = atoi(buff);
		sprintf(buff2,"AEM inserted: %d\r\n", aem);
		uart_print(buff2);
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
				
					//status start
				if (strcmp(buff, "status") == 0) {
							status_report();
							buff[0]='d';
					}

				//status stop	

        // Dispatch choice
        switch (buff[0]) {
            case 'a':
            case 'A':
							increment_sampling();
                break;
            case 'b':
            case 'B':
                decrement_sampling();
                break;
            case 'c':
            case 'C':
                 // cycle through TEMP ? HUM ? BOTH ? TEMP …
            disp_mode = (disp_mode == BOTH_MODE)
                        ? TEMP_MODE
                        : (display_mode_t)(disp_mode + 1);
            // acknowledge
            const char* names[] = { "TEMP", "HUM", "BOTH" };
            char msg[32];
            sprintf(msg, "Display mode: %s\r\n", names[disp_mode]);
            uart_print(msg);
            break;
            case 'd':
            case 'D':
							sampling = true;
							timer_enable();
							uart_print("Sampling started. Press 's' to stop.\r\n");
							break;
						case 's':
						case 'S':
							if(sampling){
								sampling = false;
								timer_disable();
								uart_print("Sampling stopped. Back to menu.\r\n");
							}else{
								uart_print("Not currently sampling.\r\n");
							}
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
		if ( (counter_button%2) == 0) {
			profile = mode_b;
			uart_print("switch to mode B\r\n");
		}
		else {
			profile = mode_a;
			uart_print("switch to mode A\r\n");
		}
		
		if ( ((counter_button+1)%3) == 0) {
			refresh_rate = ( ((aem%100 - aem%10)/10) + aem%10);
			if (refresh_rate>10) {
				refresh_rate = 10;
			}
			else if (refresh_rate <2) {
				refresh_rate = 2;
			}
			sprintf(buff2, "New refresh rate: %ds\r\n", refresh_rate);
			uart_print(buff2);
		}
        counter_button++;
    }
    gpio_set(P_DBG_ISR, 0);
}

// Periodic timer ISR
void timer_isr(void) {
    // TODO: periodic activity (e.g., call dht_print, alert_mode)
	if( (ticks%((uint64_t)refresh_rate )) == 0){
		sample_dht11();
		alert_mode();
		//uart_print("sample++;\r\n");
		free(results);
	}
	blink_alert();
	//sprintf(buff2,"tick: %llu", ticks);
	//uart_print(buff2);
	ticks++;	
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

//        // Blink or turn off LED
//        if (temperature_cnt > 0 || humidity_cnt > 0) {
//            gpio_toggle(LED_PIN);
//        } else {
//            gpio_set(LED_PIN, 0);
//        }
    }
}

void blink_alert(void) {
	if (profile == mode_b) {
			// Blink or turn off LED
		if (temperature_cnt > 0 || humidity_cnt > 0) {
				gpio_toggle(LED_PIN);
		} else {
				gpio_set(LED_PIN, 0);
		}
	}
	else {
		gpio_set(LED_PIN, 0);
	}
}
static void increment_sampling() {
    // only allow up to 10 seconds
    if (refresh_rate < 10) {
        refresh_rate++;                 // bump by 1 s (in CPU cycles)
				sprintf(buff2, "Sampling rate set to %d s\r\n", refresh_rate);
        uart_print(buff2);
    } else {
        uart_print("Error: maximum sampling interval is 10 s\r\n");
    }
}

static void decrement_sampling() {
    // only allow down to 1 second
    if (refresh_rate > 2) {
        refresh_rate--;                 // bump down by 1 s (in CPU cycles)
        sprintf(buff2, "Sampling rate set to %d s\r\n", refresh_rate);
				uart_print(buff2);
    } else {
        uart_print("Error: minimum sampling interval is 2 s\r\n");
    }
}

// Print current mode and counters if requested
void status_report(void) {
//    if (status_called) {
        if (profile == mode_a) {
            uart_print("Mode A\r\n");
        } else {
            uart_print("Mode B\r\n");
        }
				sample_dht11();
        sprintf(buff2, "Profile switches: %d\r\n", counter_button);
        uart_print(buff2);
//    }
}

// UART RX ISR: enqueue all received bytes
void uart_rx_isr(uint8_t rx) {
    queue_enqueue(&rx_queue, rx);
}
