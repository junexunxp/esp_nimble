#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "syscfg/syscfg.h"
#include "os/os.h"
#include "nimble/ble.h"
#include "nimble/nimble_opt.h"
#include "nimble/hci_common.h"
#include "nimble/ble_hci_trans.h"
#include "controller/ble_hw.h"
#include "controller/ble_ll_adv.h"
#include "controller/ble_ll_scan.h"
#include "controller/ble_ll.h"
#include "controller/ble_ll_test.h"
#include "controller/ble_ll_hci.h"
#include "controller/ble_ll_whitelist.h"
#include "controller/ble_ll_resolv.h"
#include "controller/ble_ll_sync.h"
#if MYNEWT_VAL(BLE_LL_DBG_HCI_CMD_PIN) >= 0 || MYNEWT_VAL(BLE_LL_DBG_HCI_EV_PIN) >= 0
#include "hal/hal_gpio.h"
#endif
#include "hal/hal_gpio.h"

#if MYNEWT_VAL(BLE_LL_DIRECT_TEST_MODE)
#include "ble_ll_dtm_priv.h"
#endif
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "runtest/runtest.h"
#include "os/mynewt.h"

#include "testutil/testutil.h"
#include "controller/ble_ll_test.h"
#include "runtest/runtest.h"



/* Venfor specific commands*/
#define BLE_HCI_OCF_DBG_RD_MEM                  (0x01)
#define BLE_HCI_OCF_DBG_WR_MEM                  (0x02)
#define BLE_HCI_OCF_VS_RUN_BB_TEST                  (0xF0)


#define HCI_ERR_EBADSTATE 0xF0
#define HCI_ERR_ENOENT 0xF1
#define HCI_ERR_EUNKNOWN 0xF2


/*
 * package "run test" request from newtmgr and enqueue on default queue
 * of the application which is actually running the tests (e.g., mynewtsanity).
 * Application callback was initialized by call to run_evb_set() above.
 */

void test_hci_support_pass_hci(const char *msg, void *arg)
{
    uint8_t *evbuf;

    evbuf = ble_hci_trans_buf_alloc(BLE_HCI_TRANS_BUF_EVT_HI);
    if (!evbuf) {
        return;
    }

    evbuf[0] = BLE_HCI_EVCODE_VENDOR_DEBUG;
    evbuf[1] = 2;
    /* subopcode */
    evbuf[2] = 0xF0;
    /* signal id */
    evbuf[3] = 0x00;
    ble_ll_hci_event_send((struct ble_hci_ev *)evbuf);
}

void test_hci_support_fail_hci(const char *msg, void *arg)
{
    uint8_t *evbuf;
    evbuf = ble_hci_trans_buf_alloc(BLE_HCI_TRANS_BUF_EVT_HI);
    if (!evbuf) {
        return;
    }

    evbuf[0] = BLE_HCI_EVCODE_VENDOR_DEBUG;
    evbuf[1] = 2;
    /* subopcode */
    evbuf[2] = 0xF1;
    /* signal id */
    evbuf[3] = 0x00;
    /* failure message length*/
    evbuf[1] += 1;

    /* failure message */
    if ( strlen(msg)<= 250) {
        memcpy(&evbuf[5], (uint8_t *)msg, strlen(msg));
        evbuf[1] += strlen(msg);
        evbuf[4] = strlen(msg);
    }
    else {
        memcpy(&evbuf[5], (uint8_t *)msg, 250);
        evbuf[1] += 250;
        evbuf[4] = 250;
    }
    ble_ll_hci_event_send((struct ble_hci_ev *)evbuf);
}



//Function to trigger test suit or test case using mynewt testutil
int test_hci_support_run_test_util(uint8_t *buf, uint16_t len)
{
    char testname[MYNEWT_VAL(RUNTEST_MAX_TEST_NAME_LEN)] = "";
    char token[MYNEWT_VAL(RUNTEST_MAX_TOKEN_LEN)] = "";
    int rc;

    uint8_t ts_len = buf[0];
    uint8_t token_len = buf[ts_len + 1];

    memcpy(testname, &buf[1], ts_len);
    memcpy(token, &buf[ts_len + 2], token_len);

    /*
     * testname is one of:
     * a) a specific test suite name
     * b) "all".
     * c) "" (empty string); equivalent to "all".
     *
     * token is appended to log messages.
     */
    rc = runtest_run(testname, token);
    rc = 0;
    switch (rc) {
    case 0:
        return 0;

    case SYS_EAGAIN:
        return HCI_ERR_EBADSTATE;

    case SYS_ENOENT:
        return HCI_ERR_ENOENT;

    default:
        return HCI_ERR_EUNKNOWN;
    }
   
}



uint32_t mem_addr;
uint8_t mem_access;
uint8_t mem_size;

static void test_hci_support_read_mem(void)
{
    uint8_t *evbuf;

    evbuf = ble_hci_trans_buf_alloc(BLE_HCI_TRANS_BUF_EVT_HI);
    if (!evbuf) {
        return;
    }

    evbuf[0] = BLE_HCI_EVCODE_VENDOR_DEBUG;
    evbuf[1] = 5 + mem_size;
    /* subopcode */
    evbuf[2] = 0xF5;
    put_le32(evbuf+3, mem_addr);
    memcpy(evbuf+7, (uint8_t *)mem_addr, mem_size);
    ble_ll_hci_event_send((struct ble_hci_ev *)evbuf);
   
}

void test_hci_support_start_test_cmd(void)
{
    test_hci_support_run_test_util(test_cmd_buf, test_len);
    memset(test_cmd_buf, 0 , test_len);
    test_len = 0;
}

//Function to handle vendor specific commands
int test_hci_support_vs_cmd_proc(uint8_t *cmdbuf, uint16_t ocf, uint8_t *rsplen, ble_ll_hci_post_cmd_complete_cb *cb)
{
    int rc;
    uint8_t len;

    /* Assume error; if all pass rc gets set to 0 */
    rc = BLE_ERR_INV_HCI_CMD_PARMS;

    /* Get length from command */
    len = cmdbuf[sizeof(uint16_t)];
    /* Move past HCI command header */
    cmdbuf += BLE_HCI_CMD_HDR_LEN;
    switch (ocf) {
    case BLE_HCI_OCF_DBG_RD_MEM:
        if (len == 6) {
	    mem_addr = get_le32(cmdbuf);
	    mem_access = *(cmdbuf + 4);
	    mem_size = *(cmdbuf + 5);
	    *cb = test_hci_support_read_mem;
	    rc = 0;
        }
	rc += BLE_ERR_MAX + 1;
        break;
    case BLE_HCI_OCF_DBG_WR_MEM:
        if (len > 6) {
	    mem_addr = get_le32(cmdbuf);
	    mem_access = *(cmdbuf + 4);
	    mem_size = *(cmdbuf + 5);
	    memcpy((uint8_t *)mem_addr, cmdbuf + 6, mem_size); 
	    rc = 0;
        }
	break;
    case BLE_HCI_OCF_VS_RUN_BB_TEST:
        if (len != 0) {
	    memcpy(test_cmd_buf, cmdbuf, len);
	    test_len = len;
	    *cb = test_hci_support_start_test_cmd;
        rc = 0;

        }
	rc += BLE_ERR_MAX + 1;
        break;
    }
    return rc;
}

static uint8_t wait_for_cmd;


//Test signal buffers
uint8_t debug_index = 0;
uint8_t debug_data_buf[240];
uint8_t debug_msg[100];
uint8_t debug_msg_len = 0;
void test_hci_support_send_signal(uint8_t signal_id, uint8_t data_fmt)
{

	//const char *str1 = "Write Radio"; 
	//TODO: buffer may overflow
	uint8_t *evbuf = ble_hci_trans_buf_alloc(BLE_HCI_TRANS_BUF_EVT_HI);
	if (!evbuf) {
	   return;
	}

	evbuf[0] = BLE_HCI_EVCODE_VENDOR_DEBUG;
	evbuf[1] = 2;
	/* subopcode */
	evbuf[2] = 0xF2;
	/* signal id */
	evbuf[3] = signal_id;
	/* Msg len */
	evbuf[4] = debug_msg_len;
	evbuf[1] += 1;
	/* Message */
	memcpy(&evbuf[5], debug_msg, debug_msg_len);
	evbuf[1] += debug_msg_len;
	
	/* data buf format*/    
	evbuf[5+debug_msg_len] = data_fmt;
	evbuf[1] += 1;
	/* data buf length */
	evbuf[5+debug_msg_len+1] = debug_index;
	evbuf[1] += 1;
	if(debug_index)
	{
	   memcpy(&evbuf[5+debug_msg_len+2], debug_data_buf, debug_index);
	}
	evbuf[1] += debug_index;
	ble_ll_hci_event_send(evbuf);

    //make buffer ready for next debug signal
    memset(debug_data_buf, 0, sizeof(debug_data_buf));
    debug_index = 0;
}

void test_hci_support_wait_signal(uint8_t wait_id)
{
    wait_for_cmd = 1;
	while (wait_for_cmd);
}


//should be called by hci layer when cmd received
void test_hci_clear_wait_signal(void ){
	wait_for_cmd = 0;
}

void test_hci_support_put_debug_data(uint8_t *data, uint8_t len)
{
    memcpy((uint8_t *)(debug_data_buf+debug_index), data, len);
    debug_index += len;
}

void test_hci_support_set_debug_msg(const char *msg)
{
    memcpy((uint8_t *)debug_msg, msg, strlen(msg));
    debug_msg_len = strlen(msg);
}




