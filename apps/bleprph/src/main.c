/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "hal/hal_system.h"
#include "config/config.h"
#include "split/split.h"
#if MYNEWT_VAL(BLE_SVC_DIS_FIRMWARE_REVISION_READ_PERM) >= 0
#include "bootutil/image.h"
#include "imgmgr/imgmgr.h"
#include "services/dis/ble_svc_dis.h"
#endif

/* BLE */
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "../src/ble_hs_priv.h"



#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"




/* Application-specified header. */
#include "bleprph.h"
#include "nrfx_nvmc.h"
#include "nrf52840.h"

#include "gpio_debug.h"
#include "timer_debug.h"
#define TEST_THROUGHPUT		1
#define CACHE_TEST			0
#if TEST_THROUGHPUT
static uint16_t conn_handle = 0xffff;
void throughput_run(uint16_t conn_handle);

void throughput_ccd_set(uint8_t value);
void throughput_allow_tx(uint8_t allow);
#else

void bletest_completed_pkt(uint16_t handle){
}



#endif
static int bleprph_gap_event(struct ble_gap_event *event, void *arg);


/**
 * Logs information about a connection to the console.
 */
static void
bleprph_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    MODLOG_DFLT(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    MODLOG_DFLT(INFO, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    MODLOG_DFLT(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    MODLOG_DFLT(INFO, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    MODLOG_DFLT(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                "encrypted=%d authenticated=%d bonded=%d\n",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void
bleprph_advertise(void)
{
	int rc;
    
	#if MYNEWT_VAL(BLE_EXT_ADV)
	struct ble_gap_ext_adv_params adv_params;
	struct os_mbuf *adv_data;
	#else
	
    struct ble_gap_adv_params adv_params;
	#endif
	uint8_t own_addr_type;
    struct ble_hs_adv_fields fields;
    const char *name;
    

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assiging the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
   // fields.tx_pwr_lvl_is_present = 1;
    //fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]){
        BLE_UUID16_INIT(0x00ff)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
	
	/* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
	#if MYNEWT_VAL(BLE_EXT_ADV)
	adv_params.connectable = 1;

	adv_params.primary_phy = BLE_HCI_LE_PHY_1M;
	adv_params.secondary_phy = BLE_HCI_LE_PHY_1M;
	
	adv_params.sid = 0x2;
	
	rc = ble_gap_ext_adv_configure(0, &adv_params,
                                   NULL, bleprph_gap_event, NULL);
	/* Get mbuf for adv data */
    adv_data = os_msys_get_pkthdr(32, 0);
    assert(adv_data != NULL);

    /* Fill mbuf with adv fields - structured data */
    rc = ble_hs_adv_set_fields_mbuf(&fields, adv_data);

    if (rc) {
        os_mbuf_free_chain(adv_data);
        assert(0);
    }

   

    /* Include mbuf data in advertisement */
    rc = ble_gap_ext_adv_set_data(0, adv_data);
    assert (rc == 0);
	
	rc = ble_gap_ext_adv_start(0, BLE_HS_FOREVER,0);
	#else
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
	
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);
	#endif
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        MODLOG_DFLT(INFO, "connection %s; status=%d ",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleprph_print_conn_desc(&desc);

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
            phy_conn_changed(event->connect.conn_handle);
#endif
        }
        MODLOG_DFLT(INFO, "\n");
		#if TEST_THROUGHPUT
		conn_handle = event->connect.conn_handle;
		//Modify @June
		//Set to 2M phy
		ble_gap_set_prefered_le_phy(conn_handle, 4, 4, 1);
		//struct ble_gap_upd_params params={0};
		//params.itvl_min = 32;//32;
		//params.itvl_max = 32;//32;
		//params.supervision_timeout =600;
		//ble_gap_update_params(conn_handle, &params);
		ble_hs_hci_util_set_data_len(conn_handle,251,17040);
		#endif
        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            bleprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        bleprph_print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
        phy_conn_changed(CONN_HANDLE_INVALID);
#endif

        /* Connection terminated; resume advertising. */
        bleprph_advertise();
#if TEST_THROUGHPUT

		conn_handle = 0xffff;
		throughput_ccd_set(0);
#endif
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        MODLOG_DFLT(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "advertise complete; reason=%d",
                    event->adv_complete.reason);
		if(event->adv_complete.reason)
        	bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
                          "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
		#if TEST_THROUGHPUT
		throughput_ccd_set(event->subscribe.cur_notify);
		#endif
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    case BLE_GAP_EVENT_NOTIFY_TX:
   #if TEST_THROUGHPUT
		throughput_allow_tx(1);	
   #endif
		break;

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
    case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
        /* XXX: assume symmetric phy for now */
        phy_update(event->phy_updated.tx_phy);
        return 0;
#endif
    }

    return 0;
}

static void
bleprph_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
bleprph_on_sync(void)
{
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin advertising. */
    bleprph_advertise();
}

#if CACHE_TEST
void cache_test_cb(struct os_event *ev){
	printf("cache hit %ld, cache miss %ld\n",nrf_nvmc_icache_hit_get(NRF_NVMC),nrf_nvmc_icache_miss_get(NRF_NVMC));
	nrf_nvmc_icache_hit_miss_reset(NRF_NVMC);

}
#endif




//static struct os_event cache_test_ev = {
 //   .ev_cb = cache_test_cb,
//};

/**
 * main
 *
 * The main task for the project. This function initializes the packages,
 * then starts serving events from default event queue.
 *
 * @return int NOTE: this function should never return!
 */
int
main(void)
{
#if MYNEWT_VAL(BLE_SVC_DIS_FIRMWARE_REVISION_READ_PERM) >= 0
    struct image_version ver;
    static char ver_str[IMGMGR_NMGR_MAX_VER];
#endif
    int rc;
	gpio_dbug_init();
	
    /* Initialize OS */
    sysinit();
	
	//hal_rtc_test();

	//hal_timer_test();

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    rc = gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("Throughput_test");
    assert(rc == 0);

#if MYNEWT_VAL(BLE_SVC_DIS_FIRMWARE_REVISION_READ_PERM) >= 0
    /* Set firmware version in DIS */
    imgr_my_version(&ver);
    imgr_ver_str(&ver, ver_str);
    ble_svc_dis_firmware_revision_set(ver_str);
#endif

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
    phy_init();
#endif

    conf_load();
#if CACHE_TEST
	nrfx_nvmc_icache_enable();
#endif


    /* If this app is acting as the loader in a split image setup, jump into
     * the second stage application instead of starting the OS.
     */
#if MYNEWT_VAL(SPLIT_LOADER)
    {
        void *entry;
        rc = split_app_go(&entry, true);
        if (rc == 0) {
            hal_system_start(entry);
        }
    }
#endif
    /*
     * As the last thing, process events from default event queue.
     */
    #if CACHE_TEST
	uint32_t ticks = os_cputime_get32();
	#endif
	
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
		#if TEST_THROUGHPUT
		throughput_run(conn_handle);
		#endif
		#if CACHE_TEST
		if(os_cputime_get32() - ticks > 20000){
			ticks = os_cputime_get32();
			cache_test_cb(NULL);
		}
		#endif

    }
    return 0;
}
