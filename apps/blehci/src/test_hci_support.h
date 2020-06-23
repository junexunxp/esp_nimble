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

#define TEST_SIGNAL(signal_id, data_fmt) test_hci_support_send_signal(signal_id, data_fmt)
#define TEST_WAIT(signal_id) test_hci_support_wait_signal(signal_id)

//should be called by hci layer when cmd received in function ble_ll_hci_cmd_rx
//For wvt framwork support
/*
extern void test_hci_clear_wait_signal(void );
test_hci_clear_wait_signal();
*/
//end

void test_hci_clear_wait_signal(void );

int test_hci_support_vs_cmd_proc(const uint8_t *cmdbuf, uint8_t inlen, uint16_t ocf,
							                       uint8_t *rspbuf, uint8_t *rsplen,
							                       ble_ll_hci_post_cmd_complete_cb *cb);


void test_hci_support_pass_hci(const char *msg, void *arg);
void test_hci_support_fail_hci(const char *msg, void *arg);
int test_hci_support_run_test_util(uint8_t *buf, uint16_t len);
void test_hci_support_put_debug_data(uint8_t *data, uint8_t len);
void test_hci_support_set_debug_msg(const char *msg);
void test_hci_support_send_signal(uint8_t signal_id, uint8_t data_fmt);

#endif
#endif

