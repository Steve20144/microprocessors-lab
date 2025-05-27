#include "gpio.h"
#include "platform.h"
#include "stm32f4xx.h"
#include "uart.h"
#include "dht11.h"
#include <stdio.h>


int main(void){
    static uint8_t* results;
    char buff[64];
    
    uart_init(115200);
    uart_enable();
    
    __enable_irq();
    
    dht11_poll(results);
        
        sprintf(buff, "Humidity: %d\r\nTemperature: %d\r\n", results[0], results[2]);
        uart_print(buff);
    
    while(1){
        
        
        
        
    }
}
