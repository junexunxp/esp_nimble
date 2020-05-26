#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"

#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "bleprph.h"

#if 0
//coded phy 490 +18
#define NOTIFY_LEN			495//490
uint8_t gatt_svr_throughput_static_val[NOTIFY_LEN/*241*/];
static uint16_t gatt_throuput_svr_handle = 0xffff;
static uint8_t ccd = 0;
static uint8_t allow_tx = 0;
static int
gatt_svr_chr_access_throughput(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);

void throughput_run(uint16_t conn_handle);


static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Security test. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x00ff),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Random number generator. */
            .uuid = BLE_UUID16_DECLARE(0xff01),
            .access_cb = gatt_svr_chr_access_throughput,
            .val_handle = &gatt_throuput_svr_handle,
            .flags = BLE_GATT_CHR_F_NOTIFY|BLE_GATT_CHR_F_READ,
        }, {
            /*** Characteristic: Static value. */
            .uuid =BLE_UUID16_DECLARE(0xff02),
            .access_cb = gatt_svr_chr_access_throughput,
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        0, /* No more services. */
    },
};


static int
gatt_svr_chr_access_throughput(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    const ble_uuid_t *uuid;
    int rand_num;
    int rc = 0;

    uuid = ctxt->chr->uuid;

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */
	MODLOG_DFLT(DEBUG, "access callback called\n");
	
	printf("call handle %d, throughput svr handle %d\n",attr_handle,gatt_throuput_svr_handle);
    if (ble_uuid_cmp(uuid, BLE_UUID16_DECLARE(0xff01)) == 0) {

        /* Respond with a 32-bit random number. */
        rand_num = rand();
        rc = os_mbuf_append(ctxt->om, &rand_num, sizeof rand_num);
		
		MODLOG_DFLT(DEBUG, "uuid 0xff02 called\n");
		//allow_tx = 1;
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ble_uuid_cmp(uuid, BLE_UUID16_DECLARE(0xff02)) == 0) {
		MODLOG_DFLT(DEBUG, "uuid 0xff02 called\n");
        switch (ctxt->op) {
        case BLE_GATT_CHR_F_WRITE_NO_RSP:
            return rc == 0;
        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }else{
		return 0;
    }

    /* Unknown characteristic; the nimble stack should not have called this
     * function.
     */
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}
							 
static uint32_t tx_complete_packet_num = 0xffffffff;
static uint32_t ticks = 0;
static uint8_t rslt_show_en = 0;
static uint8_t no_memory = 0;
static uint32_t time_us = 0;
static uint32_t packets_cnt = 0;

void bletest_show_rslt(void ){
	if(rslt_show_en){
	
	uint32_t txed_bytes = (packets_cnt>>1)*(sizeof gatt_svr_throughput_static_val);
	
	uint32_t cnt  = txed_bytes*1000;
	uint32_t ms = time_us/1000;
	uint32_t speed_r = cnt /ms;
	
	printf("time_us %ld, bytes txed %ld, speed: %ld Bps, %ld bps\n",time_us, txed_bytes, speed_r, speed_r*8);
	rslt_show_en = 0;
	}
	
}



void bletest_completed_pkt(uint16_t handle){
	no_memory = 0;
	if(tx_complete_packet_num == 0xffffffff){
		ticks = os_cputime_get32();
		tx_complete_packet_num++;
	}else{
			tx_complete_packet_num++;
			if(tx_complete_packet_num > 499){
				time_us = os_cputime_ticks_to_usecs((uint32_t )(os_cputime_get32() - ticks));
				packets_cnt = tx_complete_packet_num;
				rslt_show_en = 1;
				tx_complete_packet_num = 0xffffffff;
			}
		
	}

}

static uint8_t check_sum(uint8_t *addr, uint16_t count)
{
    uint32_t sum = 0;

    if (addr == NULL || count == 0) {
        return 0;
    }

    for(int i = 0; i < count; i++) {
        sum = sum + addr[i];
    }

    while (sum >> 8) {
        sum = (sum & 0xff) + (sum >> 8);
    }

    return (uint8_t)~sum;
}

int
gatt_svr_chr_notify_throughput(uint16_t conn_handle)
{
	int rc = -1;

	if((conn_handle != 0xffff) && (gatt_throuput_svr_handle!= 0xffff) && (ccd == 1) && (allow_tx) &&(!no_memory)){
	
		#if 0
		struct ble_att_notify_req {
			uint16_t banq_handle;
		} __attribute__((packed));
		struct ble_att_notify_req *req;
		struct os_mbuf *txom2;
		int rc;
		extern void *
		ble_att_cmd_get(uint8_t opcode, size_t len, struct os_mbuf **txom);
		req = ble_att_cmd_get(BLE_ATT_OP_NOTIFY_REQ, sizeof(*req), &txom2);
		if (req == NULL) {
			return BLE_HS_ENOMEM;
		}
	
		req->banq_handle = htole16(gatt_throuput_svr_handle);
		os_mbuf_copyinto(txom2, 0, gatt_svr_throughput_static_val, sizeof gatt_svr_throughput_static_val);
		printf("overall send len %d\n",OS_MBUF_PKTHDR(txom2)->omp_len);
		extern int
		ble_att_tx(uint16_t conn_handle, struct os_mbuf *txom);
		rc = ble_att_tx(conn_handle, txom2);
		return rc;

		#else
		uint8_t jk = 4;
		while(jk){
#if DEBUG_TIMING
		throughput_pins_toggle(0);
#endif
		struct os_mbuf *om;
		om = ble_hs_mbuf_from_flat(gatt_svr_throughput_static_val, sizeof gatt_svr_throughput_static_val);
		if(om == NULL){
			//printf("no available memory\r\n");
#if DEBUG_TIMING
			throughput_pins_toggle(2);
#endif
			no_memory = 1;
			break;
		}
		//printf("1. len %d\n",OS_MBUF_PKTLEN(om));
		allow_tx = 0;
		rc = ble_gattc_notify_custom(conn_handle, gatt_throuput_svr_handle, om);
		if(rc != 0){
#if DEBUG_TIMING
			throughput_pins_toggle(1);
#endif

			//printf("no available memory\r\n");
			no_memory = 1;
			break;
		}
		jk--;
#if DEBUG_TIMING
		throughput_pins_toggle(0);
#endif
		}
		#endif
		bletest_show_rslt();
		
	}
	return rc;
}
void throughput_ccd_set(uint8_t value){
	allow_tx = value;

	ccd = value;

}

void throughput_allow_tx(uint8_t allow){
	allow_tx = allow;


}



void throughput_run(uint16_t conn_handle){

	gatt_svr_chr_notify_throughput(conn_handle);
}

					 

int throughput_test_init(void ){
	int rc;
	uint16_t cnt=0;
	for(;cnt<NOTIFY_LEN;cnt++){
		gatt_svr_throughput_static_val[cnt] = cnt&0x00ff;
	}
	//ble_gap_set_prefered_default_le_phy(7,7);
	gatt_svr_throughput_static_val[NOTIFY_LEN -1] = check_sum(gatt_svr_throughput_static_val,NOTIFY_LEN-1);
	printf("notify value last 4 bytes 0x%x, 0x%x 0x%x, 0x%x\r\n",gatt_svr_throughput_static_val[NOTIFY_LEN-4],gatt_svr_throughput_static_val[NOTIFY_LEN-3],
																gatt_svr_throughput_static_val[NOTIFY_LEN-2],gatt_svr_throughput_static_val[NOTIFY_LEN-1]);

	rc = ble_gatts_count_cfg(gatt_svr_svcs);
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_add_svcs(gatt_svr_svcs);
	return rc;
	
	
}

#endif
