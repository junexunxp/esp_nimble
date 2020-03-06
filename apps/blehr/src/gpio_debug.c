#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
static const int dbug_gpio[8]={0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28};

//static  uint8_t gpio_v[8] = {1};
void gpio_toggle(uint8_t gpio_indx){
    if(gpio_indx<sizeof(dbug_gpio)){
       // hal_gpio_write(dbug_gpio[gpio_indx],!gpio_v[gpio_indx]);
        hal_gpio_toggle(dbug_gpio[gpio_indx]);
       // gpio_v[gpio_indx] = !gpio_v[gpio_indx];
    }
}

void
gpio_dbug_init(void)
{
    uint8_t i=0;
    for(;i<8;i++){
        hal_gpio_init_out(dbug_gpio[i], 1);
    }
    
}