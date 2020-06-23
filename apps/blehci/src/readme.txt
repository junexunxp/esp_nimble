All of our test work should be done in master branch, so make sure repos children folders all in master branch.

Porting Guide to support WVT test framework:

1. Copy wvt_port.c and wvt_port.h files to your working project folder, e.g. apache-mynewt-nimble/apps/blehci/src

2. Include wvt_port.h file in your own test case file.

3. In your own test case file(.c), add following codes:
	//3.1 Test case declare
	TEST_CASE(hal_radio_tc_01)
	{
		//main test routine
	}

	//3.2 Declare your test suite for your own type test cases,e.g.
	TEST_SUITE(hal_radio_tc)
	{
	    hal_radio_tc_01();
	    //hal_radio_tc_02();
		//hal_radio_tc_03();
	}

	
	//3.3 Implement the test case init function, which should be called after sys init in main function
	void hal_radio_tc_init(void)
	{
	    //runtest_init();
	    TEST_SUITE_REGISTER(hal_radio_tc);
	    tu_set_pass_cb(wvt_port_pass_hci, NULL);
	    tu_set_fail_cb(wvt_port_fail_hci, NULL);
	}


4. Add below code block to file apache-mynewt-nimble/nimble/controller/src/ble_ll_hci.c line 1588
	//Receive test commands from WVT. Support wvt framework
	case BLE_HCI_OGF_VENDOR:
		{
		extern int wvt_port_vs_cmd_proc(const uint8_t *cmdbuf, uint8_t inlen, uint16_t ocf,
							                       uint8_t *rspbuf, uint8_t *rsplen,
							                       ble_ll_hci_post_cmd_complete_cb *cb);
		rc = wvt_port_vs_cmd_proc(cmd->data, cmd->length, (uint16_t)ocf, rspbuf, &rsplen, &post_cb);
		}
		break;
	//end

5. Call test_hci_clear_wait_signal() apache-mynewt-nimble/nimble/controller/src/ble_ll_hci.c line 1699


6. Modify the example's syscfg.yml file add "OS_MAIN_STACK_SIZE: 4096" to the bottom of the file.
