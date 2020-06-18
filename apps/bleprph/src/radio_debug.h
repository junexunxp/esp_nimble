#ifndef RADIO_TX_TEST_H
#define RADIO_TX_TEST_H
#include "gpio_debug.h"
#include "timer_debug.h"
void hal_radio_tx_test(void );
void hal_radio_tasks_tx(void );
void hal_radio_rx_test(void );
void hal_radio_rssi_test(void );
void hal_radio_bcc_test(void );
void hal_radio_dev_match_test(void );
void hal_radio_crc_test(void );
void hal_radio_coded_tx_test(void );
void hal_radio_coded_phyend_test(void );
void hal_radio_shorts_test_tx(void );

void hal_radio_shorts_test_rx(void );
void hal_radio_test_bcc_event_generation(void );

void hal_radio_test_endian_rx(void );
void hal_radio_test_endian_tx(void );
void hal_radio_ccm_tx_test(void );
void hal_radio_ccm_rx_test(void );

void hal_radio_rxbuffer_congestion_test(void );
void hal_ccm_endecrypt_test(void );
void hal_radio_tx_inten_test(void );

void hal_radio_rxaddress_test(void );
void hal_radio_txaddress_test(void );
void hal_radio_length_test_tx(void );
void hal_radio_length_test_rx(void );


void hal_radio_txaddress_balen_test(void );
void hal_radio_rxaddress_balen_test(void );
void hal_radio_tx_tasks_stop_test(void );


void hal_radio_rxpdustat_test(void );
void hal_radio_crccnf_test_rx(void );
void hal_radio_crccnf_test_tx(void );
void hal_radio_tifs_test_rx(void );
void hal_radio_tifs_test_tx(void );
void hal_radio_txtimmings_test(void );



#define WAIT_FOR_EVENT_ARRIVE_AND_CLR(event)	\
			do{					\
				volatile uint32_t events_v = event;	\
				if(events_v){				\
					event = 0;				\
					break;					\
				}							\
			}while(1)		\


#define REPEAT_N_TIMES_START(n)		\
	for(uint8_t rnt =0; rnt<n; rnt++)	\
		{			
#define REPEAT_N_TIMES_END()		\
		}	\

#endif
