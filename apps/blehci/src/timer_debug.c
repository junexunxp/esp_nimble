#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "hal/hal_system.h"
//#include "config/config.h"

/* Application-specified header. */
//#include "bleprph.h"
#include "nrfx_nvmc.h"
#include "nrf52840.h"
#include "gpio_debug.h"
#include "timer_debug.h"

#define DEBUG_PPI_TRIGGER_INFO			0



//Only for arm architecture

#define TICKS_PER_US					64

__attribute__((section(".data"))) void  timer_delay_us(uint16_t delay_us){
	//lsls r0,r0,#6		//1 cycle
	int tick = ((uint32_t )delay_us)*TICKS_PER_US;
	//subs r0,#8	//1 cycle
	//PUSH 1 + 2
	//POP and RETURN 1 + 2 + 3
	//b.n false 2
	tick -= 12;//add 6 extra function call delay cycles
	while(tick > 8){
		//cmp r0,#0 	//1 cycle
		//ble.n c604	//3
		tick -= 8;
		//subs 1
		//b.n 1
	}//2 instruction 2-cycle

}


__attribute__((section(".data"))) void  timer_delay_ms(uint16_t delay_ms){
	
	int tick = delay_ms*1000*TICKS_PER_US;
	while(tick>0){
		tick -= 9;
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

static void hal_rtc_timer_init(uint8_t timer_indx){
	NRF_RTC_Type *rtc = NULL;
	switch(timer_indx){
		case 0:
			rtc = NRF_RTC0;
		break;
		case 1:
			rtc = NRF_RTC1;
		break;
		case 2:
			rtc = NRF_RTC2;
		break;
		default:
		break;
	}
	if(rtc){
		rtc->EVTENSET = 1;
		rtc->TASKS_START = 1;
	}



}




void hal_rtc_ppi_clear(void ){
	NRF_RTC0->EVTENSET = 1;
	NRF_RTC0->TASKS_START = 1;

	volatile uint32_t counter = NRF_RTC0->COUNTER;
	timer_delay_ms(500);

	printf("%ld\n",counter);


	while(1){
		NRF_PPI->CH[7].EEP = (uint32_t)&(NRF_RTC0->EVENTS_TICK);
		NRF_PPI->CH[7].TEP = (uint32_t)&(NRF_RTC0->TASKS_CLEAR);
		NRF_PPI->CHENSET = PPI_CHEN_CH7_Msk;
		timer_delay_ms(100);
		printf("counter 2 %ld\n",NRF_RTC0->COUNTER);
	}



}




void hal_ppi_register_test(void ){
	/*NRF_PPI->CHG[0] = 0x0F;
	printf("1 chg v: %ld\n",NRF_PPI->TASKS_CHG[0].EN);

	NRF_PPI->TASKS_CHG[0].EN = 1;
	printf("2 chg v: %ld\n",NRF_PPI->TASKS_CHG[0].EN);
	*/
	NRF_RADIO->POWER=1;
	NRF_RADIO->TASKS_TXEN = 1;
	while(!NRF_RADIO->EVENTS_READY);
	NRF_RADIO->EVENTS_READY=0;
	printf("NRF_RADIO->EVENTS_READY %ld\n",NRF_RADIO->EVENTS_RXREADY);

}


void hal_ppi_events_reg_test(void ){
	NRF_PPI->CH[7].EEP = (uint32_t)&(NRF_RTC0->EVENTS_TICK);
	volatile uint32_t eepvalue = (uint32_t )NRF_PPI->CH[7].EEP;
	printf("eepvalue 0x%lx\n",eepvalue);
	NRF_PPI->CH[7].EEP = (uint32_t)&(NRF_RTC1->EVENTS_TICK);
	eepvalue = (uint32_t )NRF_PPI->CH[7].EEP;
	printf("eepvalue 0x%lx\n",eepvalue);



}


void hal_rtc_test(void ){
//
//	NRF_RTC0->INTENSET = 1;
	hal_rtc_timer_init(0);
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
			//debug internal module PPI functions

			while(1){
				NRF_PPI->CH[7].EEP = (uint32_t)&(NRF_RTC0->EVENTS_TICK);
				NRF_PPI->CH[7].TEP = (uint32_t)&(NRF_RTC0->TASKS_CLEAR);
				NRF_PPI->CHENSET = PPI_CHEN_CH7_Msk;
				timer_delay_ms(100);
				printf("counter 2 %ld\n",NRF_RTC0->COUNTER);
			}
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

