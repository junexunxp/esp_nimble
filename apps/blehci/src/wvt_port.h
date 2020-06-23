#ifndef TEST_HCI_SUPPORT_H
#define TEST_HCI_SUPPORT_H

#if TEST_USE_WVT
#include "controller/ble_ll_hci.h"

#include "testutil/testutil.h"
#include "runtest/runtest.h"

#define SIGNAL_ID_DEFAULT   0xff
#define DATA_FMT_STRING		0xff
#define DATA_FMT_WORD		4
#define DATA_FMT_HALF_WORD	2
#define DATA_FMT_BYTE		1


//Send dbg messages to wvt
#define SET_DBG_MSG(msg)						wvt_port_set_debug_msg(msg)


//Send dbg data to wvt
#define SET_DBG_DATA(data,len)					wvt_port_put_debug_data(data,len)

//Notify packet to wvt
#define TEST_SIGNAL(signal_id, data_fmt) 		wvt_port_send_signal(signal_id, data_fmt)
#define TEST_WAIT(signal_id) 					wvt_port_wait_signal(signal_id)

//should be called by hci layer when cmd received in function ble_ll_hci_cmd_rx
//For wvt framwork support
/*
extern void test_hci_clear_wait_signal(void );
test_hci_clear_wait_signal();
*/
//end

/*
	@bref wait signal clear function which called in function ble_ll_hci_cmd_rx
*/
void test_hci_clear_wait_signal(void );


/*

	@bref subroutine to handle vendor messages delivered by wvt
*/
int wvt_port_vs_cmd_proc(const uint8_t *cmdbuf, uint8_t inlen, uint16_t ocf,
							                       uint8_t *rspbuf, uint8_t *rsplen,
							                       ble_ll_hci_post_cmd_complete_cb *cb);


/*
	@bref pass callback function by runtest

*/
void wvt_port_pass_hci(const char *msg, void *arg);


/*
	@bref fail callback function by runtest

*/
void wvt_port_fail_hci(const char *msg, void *arg);


/*
	@bref store debug data which will be txed to host in signal function.
		  use SET_DBG_DATA instead

*/

void wvt_port_put_debug_data(uint8_t *data, uint8_t len);


/*
	@bref store debug msg which will be txed to host in signal function.
		  use SET_DBG_MSG instead

*/

void wvt_port_set_debug_msg(const char *msg);

/*
	@bref Signal host the changes
		  use TEST_SIGNAL instead

*/

void wvt_port_send_signal(uint8_t signal_id, uint8_t data_fmt);

#endif
#endif

