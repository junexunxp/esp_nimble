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

static inline void
ble_phy_dbg_setup_gpiote(int index, int pin)
{
    NRF_GPIO_Type *port;
    port = pin > 31 ? NRF_P1 : NRF_P0;
    pin &= 0x1f;
    /* Configure GPIO directly to avoid dependency to hal_gpio (for porting) */
    port->DIRSET = (1 << pin);
    port->OUTSET = (1 << pin);

    NRF_GPIOTE->CONFIG[index] =
                        (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
                        ((pin & 0x1F) << GPIOTE_CONFIG_PSEL_Pos) |
                        ((port == NRF_P1) << GPIOTE_CONFIG_PORT_Pos);
}

static void
ble_phy_dbg_setup_ppi(void)
{
    int gpiote_idx __attribute__((unused)) = 8;
#if DEBUG_USINGPPI
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
	
	NRF_PPI->CHENSET = PPI_CHEN_CH7_Msk | PPI_CHEN_CH8_Msk | PPI_CHEN_CH9_Msk | PPI_CHEN_CH10_Msk  | PPI_CHEN_CH12_Msk;

#endif
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
