#ifndef TEST_HCI_SUPPORT_H
#define TEST_HCI_SUPPORT_H

#if TEST_USE_WVT
#include "testutil/testutil.h"
#include "runtest/runtest.h"

#define TEST_SIGNAL(signal_id, data_fmt) test_hci_support_send_signal(signal_id, data_fmt)
#define TEST_WAIT(signal_id) test_hci_support_wait_signal(signal_id)

//should be called by hci layer when cmd received
void test_hci_clear_wait_signal(void );


int test_hci_support_vs_cmd_proc(uint8_t *cmdbuf, uint16_t ocf, uint8_t *rsplen, ble_ll_hci_post_cmd_complete_cb *cb);
void test_hci_support_pass_hci(const char *msg, void *arg);
void test_hci_support_fail_hci(const char *msg, void *arg);
int test_hci_support_run_test_util(uint8_t *buf, uint16_t len);

#endif
#endif

