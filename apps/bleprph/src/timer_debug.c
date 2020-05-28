#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "hal/hal_system.h"
#include "config/config.h"

/* Application-specified header. */
#include "bleprph.h"
#include "nrfx_nvmc.h"
#include "nrf52840.h"
#include "gpio_debug.h"

#define DEBUG_PPI_TRIGGER_INFO			1

#define TICKS_PER_US					64

//Only for arm architecture

__attribute__( ( section(".data") ) ) void timer_delay_us(uint16_t delay_us){
	if(!delay_us){//1 isntruction 1 -cycle
		return;
	}
	uint32_t tick = delay_us*TICKS_PER_US;
	while(tick){
		tick -= 2;
	}//2 instruction 2-cycle

}


__attribute__( ( section(".data") ) ) void timer_delay_ms(uint16_t delay_ms){

	if(!delay_ms){//1 isntruction 1 -cycle
		return;
	}
	
	uint32_t tick = delay_ms*1000*TICKS_PER_US;
	while(tick){
		tick -= 2;
	}//2 instruction 2-cycle
}


static void
hal_rtc_timer_irq_handler(void )
{
    uint32_t overflow;
    uint32_t compare;
    uint32_t tick;


    compare = NRF_RTC0->EVENTS_COMPARE[0];
    if (compare) {
		printf("compare..\n");
       NRF_RTC0->EVENTS_COMPARE[0] = 0;
    }

    tick = NRF_RTC0->EVENTS_TICK;
    if (tick) {
		printf("tick..\n");
        NRF_RTC0->EVENTS_TICK = 0;
    }

    overflow = NRF_RTC0->EVENTS_OVRFLW;
    if (overflow) {
		printf("overflow..\n");
        NRF_RTC0->EVENTS_OVRFLW = 0;
    }

    /* Recommended by nordic to make sure interrupts are cleared */
    compare = NRF_RTC0->EVENTS_COMPARE[0];
}

void hal_rtc_test(void ){
//
//	NRF_RTC0->INTENSET = 1;
	NRF_RTC0->EVTENSET = 1;
	NRF_RTC0->TASKS_START = 1;
	NVIC_EnableIRQ(RTC0_IRQn);
	NVIC_SetPriority(RTC0_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
	NVIC_SetVector(RTC0_IRQn, (uint32_t)hal_rtc_timer_irq_handler);
	volatile uint32_t counter = NRF_RTC0->COUNTER;
	while(1){
		
		volatile uint32_t flag = NRF_RTC0->EVENTS_TICK;
		if(flag){
			printf("%ld\n",counter);
			NRF_RTC0->CC[0] = counter + 100;
			volatile uint32_t cflag = NRF_RTC0->EVENTS_COMPARE[0];
			while(!cflag){
				timer_delay_us(15);
				cflag = NRF_RTC0->EVENTS_COMPARE[0];
#if DEBUG_PPI_TRIGGER_INFO
				gpio_dbg_tmr_ppi();
#else
				printf("tick event set %ld\n",NRF_RTC0->EVENTS_TICK);
#endif
			}
			
			while(1){
				volatile int i = 0xfffffff;
				while(i--);
				printf("EVENTS COMPARE CLEARED %ld\n",NRF_RTC0->EVENTS_COMPARE[0]);
			}
		}else{
			
			if(!counter || NRF_RTC0->COUNTER - counter > 100){
				printf("normal routine, counter %ld\n",counter);
				 counter = NRF_RTC0->COUNTER;
			}
			timer_delay_ms(2000);
		}


	}

}


void hal_timer_test(void ){
//
//	NRF_RTC0->INTENSET = 1;

	NRF_TIMER0->TASKS_START = 1;
	 NRF_RTC0->COUNTER;
	/* Force a capture of the timer into 'cntr' capture channel; read it */
	   NRF_TIMER0->TASKS_CAPTURE[1] = 1;
	   volatile uint32_t counter  = NRF_TIMER0->CC[1];

	while(1){
			printf("%ld\n",counter);
			NRF_TIMER0->CC[0] = counter + 5000;
			volatile uint32_t cflag = NRF_TIMER0->EVENTS_COMPARE[0];
			while(!cflag){
				volatile int i = 0xf;
				while(i--);
				NRF_TIMER0->TASKS_CAPTURE[1] = 1;
	    		counter  = NRF_TIMER0->CC[1];
				printf("c %ld\n",counter);
				cflag = NRF_TIMER0->EVENTS_COMPARE[0];
			}
			
			while(1){
				volatile int i = 0xffffff;
				while(i--);
				printf("EVENTS COMPARE CLEARED %ld\n",NRF_TIMER0->EVENTS_COMPARE[0]);
			}
		}



}

