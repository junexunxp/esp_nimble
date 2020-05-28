#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "nrfx.h"
#include "hal/hal_gpio.h"
#include "hal/hal_timer.h"
#include "gpio_debug.h"
static const int dbug_gpio[8]={0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28};

//static  uint8_t gpio_v[8] = {1};
void gpio_toggle(uint8_t gpio_indx){
    if(gpio_indx<sizeof(dbug_gpio)){
       // hal_gpio_write(dbug_gpio[gpio_indx],!gpio_v[gpio_indx]);
        hal_gpio_toggle(dbug_gpio[gpio_indx]);
       // gpio_v[gpio_indx] = !gpio_v[gpio_indx];
    }
}

void gpio_set(uint8_t gpio_indx){
    if(gpio_indx<sizeof(dbug_gpio)){
       // hal_gpio_write(dbug_gpio[gpio_indx],!gpio_v[gpio_indx]);
        hal_gpio_write(dbug_gpio[gpio_indx],1);
       // gpio_v[gpio_indx] = !gpio_v[gpio_indx];
    }
}

void gpio_clr(uint8_t gpio_indx){
    if(gpio_indx<sizeof(dbug_gpio)){
       // hal_gpio_write(dbug_gpio[gpio_indx],!gpio_v[gpio_indx]);
        hal_gpio_write(dbug_gpio[gpio_indx],0);
       // gpio_v[gpio_indx] = !gpio_v[gpio_indx];
    }
}


static inline void
ble_phy_dbg_setup_gpiote(int index, int pin)
{
	if(index>=sizeof(dbug_gpio)){
		return;
	}

    NRF_GPIO_Type *port;
    port = pin > 31 ? NRF_P1 : NRF_P0;
	#if 0
    pin &= 0x1f;
    /* Configure GPIO directly to avoid dependency to hal_gpio (for porting) */
    port->DIRSET = (1 << pin);
 	port->OUTSET = (0 << pin);
	
#endif
    NRF_GPIOTE->CONFIG[index] =
                        (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
                        ((pin & 0x1F) << GPIOTE_CONFIG_PSEL_Pos) |
                        ((port == NRF_P1) << GPIOTE_CONFIG_PORT_Pos)|
                        (GPIOTE_CONFIG_POLARITY_Toggle<<GPIOTE_CONFIG_POLARITY_Pos)|
                        (GPIOTE_CONFIG_OUTINIT_High<<GPIOTE_CONFIG_OUTINIT_Pos);
}


static inline void
ble_phy_dbg_setup_gpio_event(int index, int pin)
{
	if(index>=sizeof(dbug_gpio)){
		return;
	}

    NRF_GPIO_Type *port;
    port = pin > 31 ? NRF_P1 : NRF_P0;
	port->PIN_CNF[pin&0x1f] = GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos;
	#if 0
    pin &= 0x1f;
    /* Configure GPIO directly to avoid dependency to hal_gpio (for porting) */
    port->DIRCLR = (1 << pin);
	//port->PIN_CNF[pin] = GPIO_PIN_CNF_PULL_Pulldown << GPIO_PIN_CNF_PULL_Pos;
#endif
    NRF_GPIOTE->CONFIG[index] =
                        (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) |
                        ((pin & 0x1F) << GPIOTE_CONFIG_PSEL_Pos) |
                        ((port == NRF_P1) << GPIOTE_CONFIG_PORT_Pos)|
                        (GPIOTE_CONFIG_POLARITY_Toggle<<GPIOTE_CONFIG_POLARITY_Pos);
}

static void
ble_phy_dbg_setup_ppi(void)
{
    int gpiote_idx __attribute__((unused)) = 8;
	return;
    /*
     * We setup GPIOTE starting from last configuration index to minimize risk
     * of conflict with GPIO setup via hal. It's not great solution, but since
     * this is just debugging code we can live with this.
     */

    ble_phy_dbg_setup_gpiote(--gpiote_idx, dbug_gpio[DEBUG_ACCESSADDR_GPIO_INDX]);
	
	NRF_PPI->CH[7].EEP = (uint32_t)&(NRF_RADIO->EVENTS_ADDRESS);
	NRF_PPI->CH[7].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_CLR[gpiote_idx]);

	
	NRF_PPI->FORK[9].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_SET[gpiote_idx]);
	
    NRF_PPI->CHENSET = PPI_CHEN_CH17_Msk;

    ble_phy_dbg_setup_gpiote(--gpiote_idx,
                              dbug_gpio[DEBUG_EVENTSEND_GPIO_INDX]);
	NRF_PPI->CH[8].EEP = (uint32_t)&(NRF_RADIO->EVENTS_END);
  	NRF_PPI->CH[8].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_CLR[gpiote_idx]);
	NRF_PPI->FORK[11].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_SET[gpiote_idx]);
	

    ble_phy_dbg_setup_gpiote(--gpiote_idx,
                              dbug_gpio[DEBUG_EVENTSDIS_GPIO_INDX]);
	NRF_PPI->CH[9].EEP = (uint32_t)&(NRF_RADIO->EVENTS_DISABLED);
	NRF_PPI->CH[9].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_CLR[gpiote_idx]);
	NRF_PPI->FORK[12].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_SET[gpiote_idx]);

	ble_phy_dbg_setup_gpiote(--gpiote_idx,
                              dbug_gpio[DEBUG_TXEN_GPIO_INDX]);
	NRF_PPI->FORK[20].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_CLR[gpiote_idx]);
	
	NRF_PPI->CH[10].EEP = (uint32_t)&(NRF_RADIO->EVENTS_DISABLED);
	NRF_PPI->CH[10].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_SET[gpiote_idx]);


	ble_phy_dbg_setup_gpiote(--gpiote_idx,
                              dbug_gpio[DEBUG_CRCEND_GPIO_INDX]);
	
	NRF_PPI->CH[11].EEP = (uint32_t)&(NRF_RADIO->EVENTS_BCMATCH);
	NRF_PPI->CH[11].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_CLR[gpiote_idx]);
	NRF_PPI->FORK[7].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_SET[gpiote_idx]);

	ble_phy_dbg_setup_gpiote(--gpiote_idx,
                              dbug_gpio[DEBUG_DUPLICATE_GPIO_INDX]);
	NRF_PPI->CH[12].EEP = (uint32_t)&(NRF_RADIO->EVENTS_BCMATCH);
	NRF_PPI->CH[12].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_CLR[gpiote_idx]);
	NRF_PPI->FORK[8].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_SET[gpiote_idx]);
	//NRF_PPI->CHENSET = PPI_CHEN_CH7_Msk | PPI_CHEN_CH8_Msk | PPI_CHEN_CH9_Msk | PPI_CHEN_CH10_Msk  | PPI_CHEN_CH12_Msk;
	NRF_PPI->CHG[0] = PPI_CHEN_CH7_Msk | PPI_CHEN_CH8_Msk | PPI_CHEN_CH9_Msk | PPI_CHEN_CH10_Msk  | PPI_CHEN_CH12_Msk;
	NRF_PPI->CHG[1] = PPI_CHEN_CH7_Msk | PPI_CHEN_CH8_Msk | PPI_CHEN_CH9_Msk | PPI_CHEN_CH10_Msk  | PPI_CHEN_CH12_Msk;

	
	NRF_PPI->TASKS_CHG[1].DIS = 1;
	NRF_PPI->TASKS_CHG[0].EN = 1;

}


void gpio_dbg_access_addr_ppi_setup_tx(void ){

	if(DEBUG_ACCESSADDR_GPIO_INDX < sizeof(dbug_gpio)){
		
		ble_phy_dbg_setup_gpiote(7, dbug_gpio[DEBUG_EVENTS_READY_GPIO_INDX]);
		ble_phy_dbg_setup_gpiote(6, dbug_gpio[DEBUG_ACCESSADDR_GPIO_INDX]);
	
		NRF_PPI->CH[7].EEP = (uint32_t)&(NRF_RADIO->EVENTS_READY);
		NRF_PPI->CH[7].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_OUT[7]);

		NRF_PPI->CH[8].EEP = (uint32_t)&(NRF_RADIO->EVENTS_ADDRESS);
		NRF_PPI->CH[8].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_OUT[6]);

		NRF_PPI->CHENSET = PPI_CHEN_CH7_Msk|PPI_CHEN_CH8_Msk;

	}

	

}

void gpio_dbg_access_addr_ppi_setup_rx(void ){

	if(DEBUG_ACCESSADDR_GPIO_INDX < sizeof(dbug_gpio)){
		ble_phy_dbg_setup_gpio_event(0, dbug_gpio[DEBUG_EVENTS_READY_GPIO_INDX]);
		ble_phy_dbg_setup_gpio_event(1, dbug_gpio[DEBUG_ACCESSADDR_GPIO_INDX]);

		NRF_PPI->CH[7].EEP = (uint32_t)&(NRF_GPIOTE->EVENTS_IN[0]);
		NRF_PPI->CH[7].TEP = (uint32_t)&(NRF_TIMER0->TASKS_CAPTURE[0]);

		NRF_PPI->CH[8].EEP = (uint32_t)&(NRF_GPIOTE->EVENTS_IN[1]);
		//TODO: Needs debug why CC[2] not work
		NRF_PPI->CH[8].TEP = (uint32_t)&(NRF_TIMER0->TASKS_CAPTURE[3]);

		NRF_PPI->CHENSET = PPI_CHEN_CH7_Msk|PPI_CHEN_CH8_Msk;
	}

}


void gpio_dbg_tmr_ppi(void ){
	static int k = 0;
	if(!k){
		NRF_PPI->CH[7].EEP = (uint32_t)&(NRF_RTC0->EVENTS_TICK);
		NRF_PPI->CH[7].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_CLR[7]);
		NRF_PPI->CHENSET = PPI_CHEN_CH7_Msk;
	}
	if(k++ & 1){
		NRF_PPI->CH[7].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_CLR[7]);

	}else{
		NRF_PPI->CH[7].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_SET[7]);
	}
}

void
gpio_dbug_init(void)
{
    uint8_t i=0;
    for(;i<8;i++){
        hal_gpio_init_out(dbug_gpio[i], 1);
    }
	ble_phy_dbg_setup_ppi();
	//motion_sensor_setup();
	
	//gpio_debug_hwtmr_init();
    
}
