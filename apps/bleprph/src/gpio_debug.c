#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_timer.h"
#include "gpio_debug.h"
static const int dbug_gpio[8]={0x24,0x24,0x24,0x24,0x25,0x26,0x27,0x28};

//static  uint8_t gpio_v[8] = {1};
void gpio_toggle(uint8_t gpio_indx){
    if(gpio_indx<sizeof(dbug_gpio)){
       // hal_gpio_write(dbug_gpio[gpio_indx],!gpio_v[gpio_indx]);
        hal_gpio_toggle(dbug_gpio[gpio_indx]);
       // gpio_v[gpio_indx] = !gpio_v[gpio_indx];
    }
}


#if 0
void
tmr0_irq_cb(void)
{
	NRF_TIMER0->EVENTS_COMPARE[0]=0;
    gpio_toggle(TMR0_INTERRUPT);
	
}

void
gpio_debug_hwtmr_init(void ){

	/* Disable IRQ, set priority and set vector in table */
    NVIC_DisableIRQ(TIMER0_IRQn);
    NVIC_SetPriority(TIMER0_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetVector(TIMER0_IRQn, (uint32_t)tmr0_irq_cb);
	NVIC_EnableIRQ(TIMER0_IRQn);
}
#endif
void
gpio_dbug_init(void)
{
    uint8_t i=0;
    for(;i<8;i++){
        hal_gpio_init_out(dbug_gpio[i], 1);
    }
	//motion_sensor_setup();
	
	//gpio_debug_hwtmr_init();
    
}