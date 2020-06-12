#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "hal/hal_system.h"
#include "config/config.h"

/* Application-specified header. */
#include "bleprph.h"
#include "nrfx_nvmc.h"
#include "nrf52840.h"
#include "radio_debug.h"

#define NRF_LFLEN_BITS          (8)
#define NRF_S0LEN               (1)
#define NRF_S1LEN_BITS          (0)
#define NRF_CILEN_BITS          (2)
#define NRF_TERMLEN_BITS        (3)

/* Maximum length of frames */
#define NRF_MAXLEN              (251)
#define NRF_BALEN               (3)     /* For base address of 3 bytes */


#define NRF_PCNF0               (NRF_LFLEN_BITS << RADIO_PCNF0_LFLEN_Pos) | \
                                (NRF_S0LEN << RADIO_PCNF0_S0LEN_Pos) | \
                                (NRF_S1LEN_BITS << RADIO_PCNF0_S1LEN_Pos)
#define NRF_PCNF0_1M            (NRF_PCNF0) | \
                                (RADIO_PCNF0_PLEN_8bit << RADIO_PCNF0_PLEN_Pos)

#define NRF_PCNF0_CODED         (NRF_PCNF0) | \
                                (RADIO_PCNF0_PLEN_LongRange << RADIO_PCNF0_PLEN_Pos) | \
                                (NRF_CILEN_BITS << RADIO_PCNF0_CILEN_Pos) | \
                                (NRF_TERMLEN_BITS << RADIO_PCNF0_TERMLEN_Pos)

#define NRF_PCNF0_2M            (NRF_PCNF0) | \
                                (RADIO_PCNF0_PLEN_16bit << RADIO_PCNF0_PLEN_Pos)
                                
uint8_t test_rx_buffer[320];
uint8_t test_tx_buffer[320];
uint8_t test_enc_buffer[320];

uint8_t test_scratch_buffer[320];

#define TEST_TX_LEN		124//248

struct nrf_ccm_data
{
    uint8_t key[16];
    uint8_t pkt_counter[8];
    uint8_t dir_bit;
    uint8_t iv[8];
} __attribute__((packed));

struct nrf_ccm_data test_ccm_data={
	.key = {0x99, 0xad, 0x1b, 0x52, 0x26, 0xa3, 0x7e, 0x3e,0x05, 0x8e, 0x3b, 0x8e, 0x27, 0xc2, 0xc6, 0x66},
	.pkt_counter = {0,0,0,0,0,0,0,0},
	.dir_bit = 1,
	.iv={0xde,0xaf,0xba,0xbe,0xba,0xdc,0xab,0x24},
};

static void hal_radio_reset(void ){
	//Reset
	NRF_RADIO->POWER = 0;
	NRF_RADIO->POWER = 1;
}
static void
ble_phy_apply_errata_102_106_107(void)
{
    /* [102] RADIO: PAYLOAD/END events delayed or not triggered after ADDRESS
     * [106] RADIO: Higher CRC error rates for some access addresses
     * [107] RADIO: Immediate address match for access addresses containing MSBs 0x00
     */
    *(volatile uint32_t *)0x40001774 = ((*(volatile uint32_t *)0x40001774) &
                         0xfffffffe) | 0x01000000;
}

static void hal_ccm_tx_setup(void ){

	NRF_CCM->ENABLE = CCM_ENABLE_ENABLE_Disabled;
	
	//timer_delay_ms(1);
	NRF_CCM->ENABLE = CCM_ENABLE_ENABLE_Enabled;
	//timer_delay_ms(1);

	uint8_t *dptr = (uint8_t *)&test_enc_buffer[0];
	uint8_t *pktptr = (uint8_t *)&test_tx_buffer[0];
	NRF_CCM->SHORTS = CCM_SHORTS_ENDKSGEN_CRYPT_Msk;
	//NRF_CCM->SHORTS = 0;
	NRF_CCM->INPTR = (uint32_t)dptr;
	NRF_CCM->OUTPTR = (uint32_t)pktptr;
	NRF_CCM->SCRATCHPTR = (uint32_t)&test_scratch_buffer[0];
	NRF_CCM->EVENTS_ERROR = 0;
	NRF_CCM->MODE = CCM_MODE_LENGTH_Msk | (CCM_MODE_DATARATE_1Mbit << CCM_MODE_DATARATE_Pos);
	//test_ccm_data.dir_bit = 0;
	NRF_CCM->CNFPTR = (uint32_t)&test_ccm_data;
	//NRF_CCM->MAXPACKETSIZE = 0xFBul;
}

static void hal_ccm_rx_setup(void ){
	NRF_CCM->ENABLE = CCM_ENABLE_ENABLE_Disabled;
	NRF_CCM->ENABLE = CCM_ENABLE_ENABLE_Enabled;
	NRF_CCM->SHORTS = 0;
    NRF_CCM->EVENTS_ERROR = 0;
    NRF_CCM->EVENTS_ENDCRYPT = 0;
	uint8_t *dptr = (uint8_t *)&test_enc_buffer[0];
	uint8_t *pktptr = (uint8_t *)&test_rx_buffer[0];
	//NRF_CCM->SHORTS = CCM_SHORTS_ENDKSGEN_CRYPT_Msk;
	NRF_CCM->INPTR = (uint32_t)dptr;
	NRF_CCM->OUTPTR = (uint32_t)(pktptr + 3);
	NRF_CCM->SCRATCHPTR = (uint32_t)&test_scratch_buffer[0];

	NRF_CCM->MODE = CCM_MODE_LENGTH_Msk | CCM_MODE_MODE_Decryption|(CCM_MODE_DATARATE_1Mbit << CCM_MODE_DATARATE_Pos);
	NRF_CCM->CNFPTR = (uint32_t)&test_ccm_data;
	//test_ccm_data.dir_bit = 1;
	//NRF_CCM->MAXPACKETSIZE = 0xFBul;
	NRF_CCM->TASKS_KSGEN = 1;
   // NRF_PPI->CHENSET = PPI_CHEN_CH25_Msk;
}


static void hal_radio_500k_setup(void ){

	NRF_RADIO->PCNF0 = NRF_PCNF0_CODED;

	/* XXX: should maxlen be 251 for encryption? */
	NRF_RADIO->PCNF1 = NRF_MAXLEN |
					   (NRF_BALEN << RADIO_PCNF1_BALEN_Pos)|RADIO_PCNF1_WHITEEN_Msk;


	/* Set logical address 0 for TX and RX */
	NRF_RADIO->TXADDRESS  = 0;
	NRF_RADIO->RXADDRESSES	= (1 << 0);

	/* Configure the CRC registers */
	NRF_RADIO->CRCCNF = (RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos) | RADIO_CRCCNF_LEN_Three;

	/* Configure BLE poly */
	NRF_RADIO->CRCPOLY = 0x0000065B;
	NRF_RADIO->CRCINIT = 0x123456;

	NRF_RADIO->BASE0 = (0x568998);
    NRF_RADIO->PREFIX0 = (NRF_RADIO->PREFIX0 & 0xFFFFFF00) | (0x17);
	ble_phy_apply_errata_102_106_107();

	NRF_RADIO->FREQUENCY = 2;//37
	NRF_RADIO->DATAWHITEIV = 37;
	NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_LR500Kbit;




}


static void hal_radio_125k_setup(void ){

	NRF_RADIO->PCNF0 = NRF_PCNF0_CODED;

	/* XXX: should maxlen be 251 for encryption? */
	NRF_RADIO->PCNF1 = NRF_MAXLEN |
					   (NRF_BALEN << RADIO_PCNF1_BALEN_Pos)|RADIO_PCNF1_WHITEEN_Msk;


	/* Set logical address 0 for TX and RX */
	NRF_RADIO->TXADDRESS  = 0;
	NRF_RADIO->RXADDRESSES	= (1 << 0);

	/* Configure the CRC registers */
	NRF_RADIO->CRCCNF = (RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos) | RADIO_CRCCNF_LEN_Three;

	/* Configure BLE poly */
	NRF_RADIO->CRCPOLY = 0x0000065B;
	NRF_RADIO->CRCINIT = 0x123456;

	NRF_RADIO->BASE0 = (0x568998);
    NRF_RADIO->PREFIX0 = (NRF_RADIO->PREFIX0 & 0xFFFFFF00) | (0x17);
	ble_phy_apply_errata_102_106_107();

	NRF_RADIO->FREQUENCY = 2;//37
	NRF_RADIO->DATAWHITEIV = 37;
	NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_LR125Kbit;




}


static void hal_radio_2m_setup(void ){
	NRF_RADIO->PCNF0 = NRF_PCNF0_2M;
	/* XXX: should maxlen be 251 for encryption? */
	NRF_RADIO->PCNF1 = NRF_MAXLEN |
					   (NRF_BALEN << RADIO_PCNF1_BALEN_Pos)|RADIO_PCNF1_WHITEEN_Msk;

	/* Set logical address 0 for TX and RX */
	NRF_RADIO->TXADDRESS  = 0;
	NRF_RADIO->RXADDRESSES	= (1 << 0);
	/* Configure the CRC registers */
	NRF_RADIO->CRCCNF = (RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos) | RADIO_CRCCNF_LEN_Three;
	/* Configure BLE poly */
	NRF_RADIO->CRCPOLY = 0x0000065B;
	NRF_RADIO->CRCINIT = 0x123456;
	NRF_RADIO->BASE0 = (0x568998);
	NRF_RADIO->PREFIX0 = (NRF_RADIO->PREFIX0 & 0xFFFFFF00) | (0x17);
	ble_phy_apply_errata_102_106_107();

	NRF_RADIO->FREQUENCY = 2;//37
	NRF_RADIO->DATAWHITEIV = 37;
	NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_2Mbit;

}




static void hal_radio_1m_setup(void ){


	NRF_RADIO->PCNF0 = NRF_PCNF0_1M;

	/* XXX: should maxlen be 251 for encryption? */
	NRF_RADIO->PCNF1 = NRF_MAXLEN |
					   (NRF_BALEN << RADIO_PCNF1_BALEN_Pos)
					   |RADIO_PCNF1_WHITEEN_Msk;


	/* Set logical address 0 for TX and RX */
	NRF_RADIO->TXADDRESS  = 0;
	NRF_RADIO->RXADDRESSES	= (1 << 0);

	/* Configure the CRC registers */
	NRF_RADIO->CRCCNF = (RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos) | RADIO_CRCCNF_LEN_Three;

	/* Configure BLE poly */
	NRF_RADIO->CRCPOLY = 0x0000065B;
	NRF_RADIO->CRCINIT = 0x123456;

	NRF_RADIO->BASE0 = (0x568998);
    NRF_RADIO->PREFIX0 = (NRF_RADIO->PREFIX0 & 0xFFFFFF00) | (0x17);
	ble_phy_apply_errata_102_106_107();

	NRF_RADIO->FREQUENCY = 2;//37
	NRF_RADIO->DATAWHITEIV = 37;
	
	NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_1Mbit;

}

static void hal_radio_rx_init_common(void ){
	hal_radio_reset();
	hal_radio_1m_setup();
	//1
	NRF_RADIO->TASKS_RXEN = 1;
	//2
	timer_delay_us(140);
	uint8_t *ptr = (uint8_t *)test_rx_buffer;
	//3
	NRF_RADIO->PACKETPTR = (uint32_t )ptr+4;
	//4
	NRF_RADIO->TASKS_START = 1;



}

static void hal_ccm_decrypt_test(void ){
	NRF_CCM->ENABLE = CCM_ENABLE_ENABLE_Disabled;
	NRF_CCM->ENABLE = CCM_ENABLE_ENABLE_Enabled;
	NRF_CCM->SHORTS = 0;
    NRF_CCM->EVENTS_ERROR = 0;
    NRF_CCM->EVENTS_ENDCRYPT = 0;
	NRF_CCM->INPTR = (uint32_t)test_tx_buffer;
	NRF_CCM->SHORTS = 0;
	uint8_t *pktptr = (uint8_t *)test_rx_buffer;
	NRF_CCM->OUTPTR = (uint32_t)(pktptr + 3);
	memset(test_scratch_buffer,0,sizeof(test_scratch_buffer));
	NRF_CCM->SCRATCHPTR = (uint32_t)&test_scratch_buffer[0];

	NRF_CCM->MODE = CCM_MODE_LENGTH_Msk | CCM_MODE_MODE_Decryption|(CCM_MODE_DATARATE_1Mbit << CCM_MODE_DATARATE_Pos);
	NRF_CCM->CNFPTR = (uint32_t)&test_ccm_data;
	//test_ccm_data.dir_bit = 1;
	//NRF_CCM->MAXPACKETSIZE = 0xFBul;
	NRF_CCM->TASKS_KSGEN = 1;
	
	
	
	while(!NRF_CCM->EVENTS_ENDKSGEN);
	NRF_CCM->TASKS_CRYPT = 1;
	while(!NRF_CCM->EVENTS_ENDCRYPT);
	printf("evens endcrypt %ld %ld %ld\n",NRF_CCM->EVENTS_ENDCRYPT,NRF_CCM->EVENTS_ERROR,NRF_CCM->MICSTATUS);
	printf("rxcomplete\r\n");
	pktptr += 3;
	for(int i= 0;i<pktptr[1] + 3;i++){
		printf("0x%x ",pktptr[i]);
	}
	printf("\n");



}




void hal_ccm_endecrypt_test(void ){
	hal_ccm_tx_setup();
	uint8_t *tx_buffer = (uint8_t *)test_enc_buffer;
	uint16_t i = 0;
	tx_buffer[0] = 0x11;
	tx_buffer[1] = TEST_TX_LEN;
	tx_buffer[2] = 0;
	for(;i<TEST_TX_LEN;i++){
		tx_buffer[i+3] = i + 1;
	}
	
	NRF_CCM->TASKS_KSGEN = 1;
	while(!NRF_CCM->EVENTS_ENDKSGEN);
	NRF_CCM->TASKS_CRYPT = 1;
	while(!NRF_CCM->EVENTS_ENDCRYPT);
	#if 1
	
	hal_ccm_decrypt_test();
	
	#endif

	

}

#define PLEN	267
uint8_t micc0[PLEN];
uint8_t mic1[PLEN];
uint8_t mic2[PLEN];
uint8_t mic3[PLEN];
uint8_t mic4[PLEN];

void hal_radio_ccm_rx_test(void ){
	hal_radio_reset();
	hal_radio_1m_setup();
	hal_ccm_rx_setup();
	uint8_t *rx_buffer = (uint8_t *)test_enc_buffer;
	uint8_t *ptr = (uint8_t *)test_rx_buffer;
	ptr+=3;
	NRF_RADIO->PACKETPTR = (uint32_t )rx_buffer;
	NRF_RADIO->TASKS_RXEN = 1;
	NRF_RADIO->SHORTS = 0x40;
	NRF_RADIO->BCC = 48;
	while(!NRF_RADIO->EVENTS_READY);
	memcpy(micc0,test_scratch_buffer,PLEN);
	NRF_RADIO->TASKS_START = 1;
	while(!NRF_RADIO->EVENTS_ADDRESS);
	timer_delay_us(8);
	NRF_CCM->TASKS_CRYPT = 1;
	#if 1
	while(!NRF_RADIO->EVENTS_BCMATCH);
	uint8_t s0 = ptr[0];
	uint8_t s1 = ptr[1];
	uint8_t s2 = ptr[2];
	uint8_t s3 = ptr[3];
	NRF_RADIO->EVENTS_BCMATCH=0;
	NRF_RADIO->BCC = 160;
	memcpy(mic1,test_scratch_buffer,PLEN);
	while(!NRF_RADIO->EVENTS_BCMATCH);

	uint8_t s4 = ptr[4];
	uint8_t s5 = ptr[5];

	uint8_t s18 = ptr[18];
	uint8_t s19 = ptr[19];
	NRF_RADIO->EVENTS_BCMATCH=0;
	NRF_RADIO->BCC = 184;
	memcpy(mic2,test_scratch_buffer,PLEN);
	while(!NRF_RADIO->EVENTS_BCMATCH);
	uint8_t sa0 = ptr[19];
	uint8_t sa1 = ptr[20];

	uint8_t sa2 = ptr[21];
	uint8_t sa3 = ptr[22];

	while(!NRF_RADIO->EVENTS_PAYLOAD);
	uint8_t ss0 = NRF_CCM->EVENTS_ENDCRYPT;
	memcpy(mic3,test_scratch_buffer,PLEN);
	while(!NRF_RADIO->EVENTS_END);
	uint8_t ss1 = NRF_CCM->EVENTS_ENDCRYPT;
	memcpy(mic4,test_scratch_buffer,PLEN);

	
	
	
	printf("rx outputs: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",s0,s1,s2,s3,s4,s5,s18,s19,ss0,ss1,sa0,sa1,sa2,sa3);
	
	while(!NRF_CCM->EVENTS_ENDCRYPT && !NRF_CCM->EVENTS_ERROR);
	//while(!NRF_RADIO->EVENTS_END);
	printf("rxcomplete\r\n");
	for(int i= 0;i<ptr[1] + 3;i++){

		printf("0x%x ",ptr[i]);
	}
	printf("\n");
	for(int i= 0;i<rx_buffer[1] + 3;i++){

		printf("0x%x ",rx_buffer[i]);
	}
	printf("\n");
	
	printf("evens endcrypt %ld %ld %ld\n",NRF_CCM->EVENTS_ENDCRYPT,NRF_CCM->EVENTS_ERROR,NRF_CCM->MICSTATUS);

	printf("mic0: ");
	for(int i=0; i<PLEN; i++){

		printf("0x%x ",micc0[i]);
	}
	printf("\n");

	printf("mic1: ");
	for(int i=0; i<PLEN; i++){

		printf("0x%x ",mic1[i]);
	}
	printf("\n");

	printf("mic2: ");
	for(int i=0; i<PLEN; i++){

		printf("0x%x ",mic2[i]);
	}
	printf("\n");

	printf("mic3: ");
	for(int i=0; i<PLEN; i++){

		printf("0x%x ",mic3[i]);
	}
	printf("\n");

	printf("mic4: ");
	for(int i=0; i<PLEN; i++){

		printf("0x%x ",mic4[i]);
	}
	printf("\n");
	#else
	while(!NRF_RADIO->EVENTS_PAYLOAD);
	uint8_t s0 = ptr[3];
	uint8_t s1 = ptr[4];
	uint8_t s2 = ptr[5];
	uint8_t s3 = ptr[6];
	

	uint8_t ss0 = ptr[TEST_TX_LEN+2];
	uint8_t ss1 = ptr[TEST_TX_LEN+3];
	uint8_t ss2 = ptr[TEST_TX_LEN + 4];
	uint8_t ss3 = ptr[TEST_TX_LEN + 5];

	uint8_t s4 = NRF_CCM->EVENTS_ENDCRYPT;
	
	printf("rx outputs: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",s0,s1,s2,s3,ss0,ss1,ss2,ss3,s4);
	
	while(!NRF_CCM->EVENTS_ENDCRYPT && !NRF_CCM->EVENTS_ERROR);
	#endif
	
	
}



void hal_radio_ccm_tx_test(void ){
	hal_radio_reset();
	hal_radio_1m_setup();
	hal_ccm_tx_setup();
	uint8_t *tx_buffer = (uint8_t *)test_enc_buffer;
	uint16_t i = 0;
	tx_buffer[0] = 0x11;
	tx_buffer[1] = TEST_TX_LEN;
	tx_buffer[2] = 0;

	for(;i<TEST_TX_LEN;i++){
		tx_buffer[i+3] = i + 1;
	}

	uint8_t *ptr = (uint8_t *)test_tx_buffer;

	NRF_RADIO->PACKETPTR = (uint32_t )ptr;
	NRF_RADIO->TASKS_TXEN = 1;
	
	while(!NRF_RADIO->EVENTS_READY);
	NRF_CCM->TASKS_KSGEN = 1;
	timer_delay_us(100);
	NRF_RADIO->TASKS_START = 1;
	while(!NRF_RADIO->EVENTS_ADDRESS);
	uint8_t s0 = ptr[3];
	uint8_t s1 = ptr[4];
	uint8_t s2 = ptr[128];
	uint8_t s3 = ptr[248];
	uint8_t eec0 = NRF_CCM->EVENTS_ENDCRYPT;
	timer_delay_us(160);
	
	uint8_t s4 = ptr[32];
	uint8_t s5 = ptr[33];
	uint8_t s6 = ptr[200];
	uint8_t s7 = ptr[248];
	uint8_t eec1 = NRF_CCM->EVENTS_ENDCRYPT;
	printf("outputs: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",s0,s1,s2,s3,eec0,s4,s5,s6,s7,eec1);
	
}


void hal_radio_rxbuffer_congestion_test(void ){

	
	uint8_t *rx_buffer = (uint8_t *)test_rx_buffer;
	while(1){
		hal_radio_reset();
		hal_radio_1m_setup();
		memset(rx_buffer,0,NRF_MAXLEN);
		//1
		NRF_RADIO->TASKS_RXEN = 1;
		//2
		timer_delay_us(140);
		//3
		NRF_RADIO->PACKETPTR = (uint32_t )rx_buffer+4;
		//4
		NRF_RADIO->TASKS_START = 1;
		while(!NRF_RADIO->EVENTS_ADDRESS);
		volatile uint8_t readv;

		while(!NRF_RADIO->EVENTS_END){
			static uint8_t i=0;
			readv = rx_buffer[i++];	
		}
	
		printf("rxcomplete %d\r\n",readv);
		for(int i= 0;i<rx_buffer[5] + 3;i++){

			printf("0x%x ",rx_buffer[i+4]);
		}
		printf("\n");
		printf("rxlen %d\n",rx_buffer[1]);
	}


}


void hal_radio_bcc_test(void ){
	hal_radio_reset();
	hal_radio_1m_setup();
	uint8_t *tx_buffer = (uint8_t *)test_tx_buffer;
	uint16_t i = 0;
	tx_buffer[0] = 0x11;
	tx_buffer[1] = TEST_TX_LEN;
	tx_buffer[2] = 0;
	for(;i<TEST_TX_LEN;i++){
		tx_buffer[i+3] = i + 1;
	}
	//1
	NRF_RADIO->TASKS_TXEN = 1;
	//2

	timer_delay_us(140);

	//3
	NRF_RADIO->PACKETPTR = (uint32_t )tx_buffer;
	uint8_t eb0 = NRF_RADIO->EVENTS_BCMATCH;
	NRF_RADIO->BCC = 40;
	//4
	NRF_RADIO->TASKS_START = 1;
	
	while(!NRF_RADIO->EVENTS_ADDRESS);
	NRF_RADIO->TASKS_BCSTART = 1;
	timer_delay_us(40);
	uint8_t eb1 = NRF_RADIO->EVENTS_BCMATCH;
	NRF_RADIO->EVENTS_BCMATCH = 0;
	uint8_t eb2 = NRF_RADIO->EVENTS_BCMATCH;
	NRF_RADIO->BCC = 1780;
	NRF_RADIO->TASKS_BCSTOP = 1;
	timer_delay_us(1500);


	uint8_t eb4 = NRF_RADIO->EVENTS_BCMATCH;
	
	
	printf("outputs: 0x%x 0x%x 0x%x 0x%x\n",eb0,eb1,eb2,eb4);


}


void hal_radio_shorts_test_tx(void ){
	uint8_t runloop = 1;
	uint8_t change_shorts = 0;
	uint8_t oloop = 0;
	while(1){
		hal_radio_reset();
		switch (runloop){
			case 1:
				hal_radio_1m_setup();
				runloop=2;
			break;
			case 2:
				hal_radio_2m_setup();
				runloop=0;
			break;
			case 0:

				hal_radio_500k_setup();
				runloop=3;
			break;
			case 3:
				hal_radio_125k_setup();
				runloop=100 + change_shorts;
			break;
			case 100:
				change_shorts = 1;
				hal_radio_1m_setup();
				runloop=2;
			break;
			default:
				return;
			break;
				
		}
		if(change_shorts){
			NRF_RADIO->SHORTS= 0x40042;
		}else{
			NRF_RADIO->SHORTS=0x43;
		}
		NRF_RADIO->BCC = 8;
		uint8_t es0 = NRF_RADIO->STATE;
		uint8_t eb0 = NRF_RADIO->EVENTS_BCMATCH;
		uint8_t *tx_buffer = (uint8_t *)test_tx_buffer;
		uint16_t i = 0;
		tx_buffer[0] = 0x11;
		tx_buffer[1] = TEST_TX_LEN;
		tx_buffer[2] = 0;

		for(;i<TEST_TX_LEN;i++){
			tx_buffer[i+3] = i + 1;
		}
		
		NRF_RADIO->PACKETPTR = (uint32_t )tx_buffer;
		NRF_RADIO->TASKS_TXEN = 1;
		timer_delay_us(140);
		uint8_t es1 = NRF_RADIO->STATE;
		while(!NRF_RADIO->EVENTS_BCMATCH);
		while(!NRF_RADIO->EVENTS_END);
		timer_delay_us(10);
		uint8_t es3 = NRF_RADIO->STATE;
		printf("outputs %d: %d %d %d %d\n",++oloop,es0,eb0,es1,es3);
		
		for(int i= 0;i<tx_buffer[1] + 3;i++){
			printf("0x%x ",tx_buffer[i]);
		}
		printf("\n");
		
		timer_delay_ms(500);
		

	}


}


void hal_radio_shorts_test_rx(void ){
	uint8_t runloop = 1;
	uint8_t change_shorts = 0;
	uint8_t oloop = 0;
	while(1){
		hal_radio_reset();
		switch (runloop){
			case 1:
				hal_radio_1m_setup();
				runloop=2;
			break;
			case 2:
				hal_radio_2m_setup();
				runloop=0;
			break;
			case 0:

				hal_radio_500k_setup();
				runloop=3;
			break;
			case 3:
				hal_radio_125k_setup();
				runloop=100 + change_shorts;
			break;
			case 100:
				change_shorts = 1;
				hal_radio_1m_setup();
				runloop=2;
			break;
			default:
				return;
			break;
				
		}
		if(change_shorts){
			NRF_RADIO->SHORTS= 0x80152;
		}else{
			NRF_RADIO->SHORTS=0x153;
		}
		NRF_RADIO->BCC = 8;
		uint8_t es0 = NRF_RADIO->STATE;
		uint8_t eb0 = NRF_RADIO->EVENTS_BCMATCH;
		uint8_t *rx_buffer = (uint8_t *)test_rx_buffer;
		NRF_RADIO->PACKETPTR = (uint32_t )rx_buffer+3;
		NRF_RADIO->TASKS_RXEN = 1;
		timer_delay_us(140);
		uint8_t er0 = NRF_RADIO->EVENTS_RSSIEND;
		uint8_t rs0 = NRF_RADIO->RSSISAMPLE;
		uint8_t es1 = NRF_RADIO->STATE;
		while(!NRF_RADIO->EVENTS_BCMATCH);
		uint8_t er1 = NRF_RADIO->EVENTS_RSSIEND;
		uint8_t rs1 = NRF_RADIO->RSSISAMPLE;
		while(!NRF_RADIO->EVENTS_END);
		uint8_t rs2 = NRF_RADIO->RSSISAMPLE;
		uint8_t es2 = NRF_RADIO->STATE;
		timer_delay_us(10);
		uint8_t es3 = NRF_RADIO->STATE;
		printf("outputs %d: %d %d %d %d %d %d %d %d %d %d\n",++oloop,es0,eb0,er0,rs0,es1,er1,rs1,rs2,es2,es3);
		for(int i= 0;i<rx_buffer[4] + 3;i++){
			printf("0x%x ",rx_buffer[i+3]);
		}
		printf("\n");
		timer_delay_ms(50);
		

	}


}


void hal_radio_test_endian_tx(void ){
	hal_radio_reset();
	hal_radio_1m_setup();
	NRF_RADIO->PCNF1 |= (RADIO_PCNF1_ENDIAN_Big<<RADIO_PCNF1_ENDIAN_Pos);
	
	uint8_t *tx_buffer = (uint8_t *)test_tx_buffer;
	uint16_t i = 0;
	tx_buffer[0] = 0x11;
	tx_buffer[1] = TEST_TX_LEN;
	tx_buffer[2] = 0;

	for(;i<TEST_TX_LEN;i++){
		tx_buffer[i+3] = i + 1;
	}
	NRF_RADIO->SHORTS= 1;

	NRF_RADIO->PACKETPTR = (uint32_t )tx_buffer;
	NRF_RADIO->TASKS_TXEN = 1;

}

void hal_radio_test_endian_rx(void ){
	hal_radio_reset();
	hal_radio_1m_setup();
	//NRF_RADIO->PCNF1 |= (RADIO_PCNF1_ENDIAN_Big<<RADIO_PCNF1_ENDIAN_Pos);
	
	uint8_t *rx_buffer = (uint8_t *)test_rx_buffer;
	NRF_RADIO->SHORTS= 1;
	NRF_RADIO->PACKETPTR = (uint32_t )rx_buffer+4;
	NRF_RADIO->TASKS_RXEN = 1;

	while(!NRF_RADIO->EVENTS_END);
	printf("rxcomplete\r\n");
	for(int i= 0;i<rx_buffer[5] + 3;i++){

		printf("0x%x ",rx_buffer[i+4]);
	}
	printf("/n");



}



void hal_radio_test_bcc_event_generation(void ){
	uint8_t runloop = 1;
	uint8_t change_shorts = 0;
	uint8_t oloop = 0;
	while(1){
		hal_radio_reset();
		switch (runloop){
			case 1:
				hal_radio_1m_setup();
				runloop=1;
			break;
			case 2:
				hal_radio_2m_setup();
				runloop=0;
			break;
			case 0:

				hal_radio_500k_setup();
				runloop=3;
			break;
			case 3:
				hal_radio_125k_setup();
				runloop=100 + change_shorts;
			break;
			case 100:
				change_shorts = 1;
				hal_radio_1m_setup();
				runloop=2;
			break;
			default:
				return;
			break;
				
		}
		NRF_RADIO->BCC = 16;
		if(change_shorts){
			NRF_RADIO->SHORTS= 0x80152;
		}else{
			NRF_RADIO->SHORTS=0x153;
		}
		uint8_t *rx_buffer = (uint8_t *)test_rx_buffer;
		memset(rx_buffer,0,255);
		NRF_RADIO->PACKETPTR = (uint32_t )rx_buffer + 4;
		NRF_RADIO->EVENTS_BCMATCH = 0;
		NRF_RADIO->TASKS_RXEN = 1;
		while(!NRF_RADIO->EVENTS_BCMATCH);
		volatile uint8_t b0 = rx_buffer[4];
		volatile uint8_t b1 = rx_buffer[5];
		
		volatile uint8_t b2 = rx_buffer[6];
		volatile uint8_t b3 = rx_buffer[7];
		NRF_RADIO->EVENTS_BCMATCH = 0;
		NRF_RADIO->BCC = 56;
		while(!NRF_RADIO->EVENTS_BCMATCH);
		volatile uint8_t b4 = rx_buffer[8];
		volatile uint8_t b5 = rx_buffer[9];
		volatile uint8_t b6 = rx_buffer[10];
		
		volatile uint8_t b7 = rx_buffer[11];
		NRF_RADIO->EVENTS_BCMATCH = 0;
		NRF_RADIO->BCC = 83;
		while(!NRF_RADIO->EVENTS_BCMATCH);
		volatile uint8_t b8 = rx_buffer[12];
		volatile uint8_t b9 = rx_buffer[13];
		
		volatile uint8_t b10 = rx_buffer[14];
		
		volatile uint8_t b11 = rx_buffer[15];
		printf("outputs %d: %d %d %d %d %d %d %d %d %d %d %d %d\n",++oloop,b0,b1,b2,b3,b4,b5,b6,b7,b8,b9,b10,b11);
		printf("rxlen %d, rd[0] %x rd[rxlen] %x\n",rx_buffer[1], rx_buffer[0],rx_buffer[rx_buffer[1]]);
		timer_delay_ms(50);
		

	}


}



void hal_radio_coded_tx_test(void ){
	hal_radio_reset();
	hal_radio_500k_setup();

while(1){
	
	uint8_t *tx_buffer = (uint8_t *)test_tx_buffer;
	uint16_t i = 0;
	tx_buffer[0] = 0x11;
	tx_buffer[1] = TEST_TX_LEN;
	tx_buffer[2] = 0;

	for(;i<TEST_TX_LEN;i++){
		tx_buffer[i+3] = i + 1;
	}
	//1
	NRF_RADIO->TASKS_TXEN = 1;
	//2
	timer_delay_us(140);
	//3
	NRF_RADIO->PACKETPTR = (uint32_t )tx_buffer;
	//4
	NRF_RADIO->TASKS_START = 1;
	//5
	uint8_t rs0 = NRF_RADIO->STATE;
	//6
	timer_delay_us(40);
	//7
	uint8_t ea = NRF_RADIO->EVENTS_ADDRESS;
	//8
	timer_delay_us(1020);
	//9
	uint8_t ep = NRF_RADIO->EVENTS_PAYLOAD;
	//10
	timer_delay_us(24);
	//11
	uint8_t ee = NRF_RADIO->EVENTS_END;
	//12
	uint8_t rs1 = NRF_RADIO->STATE;
	printf("outputs: 0x%x 0x%x 0x%x 0x%x 0x%x\n",rs0,ea,ep,ee,rs1);
	timer_delay_ms(500);
	
	NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_LR125Kbit;
}


}


static uint8_t tx_ready,ready,addr,bcmatch,ep,ee,ed,pe;



static void hal_radio_isr(void ){
	
	if(NRF_RADIO->EVENTS_READY){
		
		ready = 1;
		NRF_RADIO->EVENTS_READY = 0;

	}
	else if(NRF_RADIO->EVENTS_TXREADY){
		tx_ready = 1;
		NRF_RADIO->EVENTS_TXREADY = 0;

	}
	else if(NRF_RADIO->EVENTS_ADDRESS){
		addr = 1;
		NRF_RADIO->EVENTS_ADDRESS = 0;

	}
	else if(NRF_RADIO->EVENTS_BCMATCH){
		bcmatch = 1;
		NRF_RADIO->EVENTS_BCMATCH = 0;

	}

	else if(NRF_RADIO->EVENTS_PAYLOAD){
		ep = 1;
		NRF_RADIO->EVENTS_PAYLOAD = 0;
	
	}
	else if(NRF_RADIO->EVENTS_END){
		ee = 1;
		NRF_RADIO->EVENTS_END = 0;
	
	}
	else if(NRF_RADIO->EVENTS_DISABLED){
		ed = 1;
		NRF_RADIO->EVENTS_DISABLED = 0;

	}else if(NRF_RADIO->EVENTS_PHYEND){
		pe = 1;
		NRF_RADIO->EVENTS_PHYEND = 0;

	}
	


}
void hal_radio_tx_inten_test(void ){
	 uint8_t loop = 5;
	 while(--loop){

		hal_radio_reset();
		timer_delay_us(100);
		tx_ready=0;
		ready=0;
		addr=0;
		bcmatch=0;
		ep=0;
		ee=0;
		ed=0;
		pe=0;
		//loop = 4;
		if(loop==4){
			hal_radio_1m_setup();
		}else if(loop==3){
			hal_radio_2m_setup();

		}else if(loop==2){
			hal_radio_125k_setup();
		
		}
		else if(loop==1){
			hal_radio_500k_setup();
		}
		
		NVIC_SetPriority(RADIO_IRQn, 0);
	    NVIC_SetVector(RADIO_IRQn, (uint32_t)hal_radio_isr);
		
		uint8_t *tx_buffer = (uint8_t *)test_tx_buffer;
		
		tx_buffer[0] = 0x11;
		tx_buffer[1] = TEST_TX_LEN;
		tx_buffer[2] = 0;

		for(uint16_t i = 0;i<TEST_TX_LEN;i++){
			tx_buffer[i+3] = i + 1;
		}
		//3
		NRF_RADIO->INTENSET = 0x0820041f;
		//NRF_RADIO->SHORTS = 0X100020;
		//4
		NRF_RADIO->BCC = 8;
		//5
		NRF_RADIO->TASKS_TXEN = 1;
		while(!ready);
		//7
		NRF_RADIO->TASKS_START = 1;
		while(!addr);
		NRF_RADIO->TASKS_BCSTART = 1;
		while(!ee);
		NRF_RADIO->TASKS_DISABLE = 1;
		while(!ed);
		printf("isr events: %d %d %d %d %d %d %d %d\n",tx_ready,ready,addr,bcmatch,ep,ee,ed,pe);
	 }
}


void hal_radio_tx_test(void ){
	hal_radio_reset();
	hal_radio_1m_setup();
	uint8_t *tx_buffer = (uint8_t *)test_tx_buffer;
	uint16_t i = 0;
	tx_buffer[0] = 0x11;
	tx_buffer[1] = TEST_TX_LEN;
	tx_buffer[2] = 0;

	for(;i<TEST_TX_LEN;i++){
		tx_buffer[i+3] = i + 1;
	}
	//1
	NRF_RADIO->TASKS_TXEN = 1;
	//2
	timer_delay_us(140);
	//3
	NRF_RADIO->PACKETPTR = (uint32_t )tx_buffer;
	//4
	NRF_RADIO->TASKS_START = 1;
	//5
	uint8_t rs0 = NRF_RADIO->STATE;
	//6
	timer_delay_us(40);
	//7
	uint8_t ea = NRF_RADIO->EVENTS_ADDRESS;
	//8
	timer_delay_us(1020);
	//9
	uint8_t ep = NRF_RADIO->EVENTS_PAYLOAD;
	//10
	timer_delay_us(24);
	//11
	uint8_t ee = NRF_RADIO->EVENTS_END;
	//12
	uint8_t rs1 = NRF_RADIO->STATE;
	printf("outputs: 0x%x 0x%x 0x%x 0x%x 0x%x\n",rs0,ea,ep,ee,rs1);


}

void hal_radio_rssi_test(void ){
	hal_radio_reset();
	hal_radio_1m_setup();
	uint8_t *rx_buffer = (uint8_t *)test_rx_buffer;
	memset(rx_buffer,0,NRF_MAXLEN);
	//1
	NRF_RADIO->TASKS_RXEN = 1;
	//2
	timer_delay_us(140);
	//3
	NRF_RADIO->PACKETPTR = (uint32_t )rx_buffer+4;

	NRF_RADIO->TASKS_START = 1;
	uint8_t rssi0 = NRF_RADIO->RSSISAMPLE;
	uint8_t ee0 = NRF_RADIO->EVENTS_RSSIEND;
	NRF_RADIO->TASKS_RSSISTART = 1;
	
	timer_delay_us(1);
	uint8_t ee1 = NRF_RADIO->EVENTS_RSSIEND;
	
	uint8_t rssi1 = NRF_RADIO->RSSISAMPLE;
	
	printf("outputs: 0x%x 0x%x 0x%x 0x%x\n",rssi0,ee0,ee1,rssi1);
	while(1){
		timer_delay_ms(100);
		printf("rssi %ld",NRF_RADIO->RSSISAMPLE);
	}


}



void hal_radio_crc_test(void ){
	
	hal_radio_rx_init_common();
	while(1){
	while(!NRF_RADIO->EVENTS_END);
	NRF_RADIO->EVENTS_END = 0;

	uint8_t ec0 = NRF_RADIO->EVENTS_CRCOK;
	NRF_RADIO->EVENTS_CRCOK=0;
	uint8_t ec1 = NRF_RADIO->EVENTS_CRCERROR;
	NRF_RADIO->EVENTS_CRCERROR =0;
	uint8_t ec2 = NRF_RADIO->CRCSTATUS;

	uint8_t ec3 = NRF_RADIO->RXCRC;
	
	
	printf("outputs: 0x%x 0x%x %d 0x%x\n",ec0,ec1,ec2,ec3);
	NRF_RADIO->CRCINIT= 0xffffff;
	NRF_RADIO->TASKS_START = 1;
}

}


void hal_radio_dev_match_test(void ){
	
	hal_radio_rx_init_common();
	//step 1 ~ 2
	for(uint8_t i=0;i<8;i++){
		NRF_RADIO->DAB[i] = 0x04030201;
		NRF_RADIO->DAP[i] = 0x0605;
	}
for(uint8_t j=1;j<8;j++){
	//step 2
	NRF_RADIO->DACNF = (1<<j);
	while(!NRF_RADIO->EVENTS_ADDRESS);
	NRF_RADIO->EVENTS_ADDRESS = 0;
	//step 5
	timer_delay_us(48);
	//step 6
	uint8_t ed0 = NRF_RADIO->EVENTS_DEVMATCH;
	//step 7
	uint8_t em0 = NRF_RADIO->EVENTS_DEVMISS;
	uint8_t dai = NRF_RADIO->DAI;
	//step 8
	NRF_RADIO->EVENTS_DEVMATCH = 0;
	//step 9
	uint8_t ed1 = NRF_RADIO->EVENTS_DEVMATCH;
	//step 10
	while(!NRF_RADIO->EVENTS_END);
	//step 11
	NRF_RADIO->EVENTS_END = 0;
	
	
	printf("outputs: 0x%x 0x%x %d 0x%x\n",ed0,em0,dai,ed1);
	uint8_t *ptr = (uint8_t *)test_rx_buffer;
	printf("rxdata: \n");
	for(uint8_t k=0;k<ptr[1];k++){
		printf("0x%x ",ptr[k]);
	}
	printf("/n");
	printf("DAB[%d] 0x%lx, DAP[%d] 0x%lx, DACNF 0x%lx\n",j,NRF_RADIO->DAB[j],j,NRF_RADIO->DAP[j],NRF_RADIO->DACNF);
	//step 12
	NRF_RADIO->TASKS_START = 1;

}


}


void hal_radio_coded_phyend_test(void ){
while(1){
	hal_radio_reset();
	hal_radio_500k_setup();
	uint8_t *rx_buffer = (uint8_t *)test_rx_buffer;
	memset(rx_buffer,0,NRF_MAXLEN);
	//1
	NRF_RADIO->TASKS_RXEN = 1;
	//2
	timer_delay_us(140);
	//3
	NRF_RADIO->PACKETPTR = (uint32_t )rx_buffer+4;
	//4
	NRF_RADIO->TASKS_START = 1;
	//5
	uint8_t rs0 = NRF_RADIO->EVENTS_RATEBOOST;
	while(!NRF_RADIO->EVENTS_ADDRESS);
	timer_delay_us(40);
	
	uint8_t ea = NRF_RADIO->EVENTS_RATEBOOST;
	NRF_RADIO->EVENTS_RATEBOOST= 0;
	
	timer_delay_us(18020);
	//9
	uint8_t ep = NRF_RADIO->EVENTS_PHYEND;
	printf("outputs: 0x%x 0x%x 0x%x\n",rs0,ea,ep);
}

}



void hal_radio_rx_test(void ){
	hal_radio_reset();
	hal_radio_1m_setup();
	uint8_t *rx_buffer = (uint8_t *)test_rx_buffer;
	memset(rx_buffer,0,NRF_MAXLEN);
	//1
	NRF_RADIO->TASKS_RXEN = 1;
	//2
	timer_delay_us(140);
	//3
	NRF_RADIO->PACKETPTR = (uint32_t )rx_buffer+4;
	//4
	NRF_RADIO->TASKS_START = 1;
	//5
	uint8_t rs0 = NRF_RADIO->STATE;
	//6
	timer_delay_us(40);
	//7
	uint8_t ea = NRF_RADIO->EVENTS_ADDRESS;
	while(!NRF_RADIO->EVENTS_ADDRESS);
	//8
	timer_delay_us(1020);
	//9
	uint8_t ep = NRF_RADIO->EVENTS_PAYLOAD;
	//10
	timer_delay_us(24);
	//11
	uint8_t ee = NRF_RADIO->EVENTS_END;
	//12
	uint8_t rs1 = NRF_RADIO->STATE;
	printf("outputs: 0x%x 0x%x 0x%x 0x%x 0x%x\n",rs0,ea,ep,ee,rs1);
	printf("rxlen %d\n",rx_buffer[1]);


}

void hal_radio_tasks_tx_ready(void ){
	hal_radio_reset();


	//1
	uint8_t rs0 = NRF_RADIO->STATE;
	//2
	NRF_RADIO->TASKS_RXEN = 1;
	//3
	uint8_t rs1 = NRF_RADIO->STATE;
	//4
	timer_delay_us(140);
	uint8_t er = NRF_RADIO->EVENTS_READY;
	uint8_t etr = NRF_RADIO->EVENTS_RXREADY;
	uint8_t rs2 = NRF_RADIO->STATE;

	NRF_RADIO->TASKS_DISABLE = 1;
	uint8_t rs3 = NRF_RADIO->STATE;
	timer_delay_us(10);
	uint8_t rs4 = NRF_RADIO->STATE;
	uint8_t ed = NRF_RADIO->EVENTS_DISABLED;
	printf("outputs: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x \n",rs0,rs1,er,etr,rs2,rs3,rs4,ed);

	

}

void hal_radio_tasks_rx_ready(void ){
//Reset
	NRF_RADIO->POWER = 0;
	NRF_RADIO->POWER = 1;

	//1
	uint8_t rs0 = NRF_RADIO->STATE;
	//2
	NRF_RADIO->TASKS_TXEN = 1;
	//3
	uint8_t rs1 = NRF_RADIO->STATE;
	//4
	timer_delay_us(140);
	uint8_t er = NRF_RADIO->EVENTS_READY;
	uint8_t etr = NRF_RADIO->EVENTS_TXREADY;
	uint8_t rs2 = NRF_RADIO->STATE;

	NRF_RADIO->TASKS_DISABLE = 1;
	uint8_t rs3 = NRF_RADIO->STATE;
	timer_delay_us(10);
	uint8_t rs4 = NRF_RADIO->STATE;
	uint8_t ed = NRF_RADIO->EVENTS_DISABLED;
	printf("outputs: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x \n",rs0,rs1,er,etr,rs2,rs3,rs4,ed);

}






