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



#endif
