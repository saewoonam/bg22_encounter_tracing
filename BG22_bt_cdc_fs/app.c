/***************************************************************************//**
 * @file app.c
 * @brief Silicon Labs Empty Example Project
 *
 * This example demonstrates the bare minimum needed for a Blue Gecko C application
 * that allows Over-the-Air Device Firmware Upgrading (OTA DFU). The application
 * starts advertising after boot and restarts advertising after a connection is closed.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
 /*******************************************************************************
 * # License
 * Copyright 2020 Sae Woo Nam
 *
 * Modified empty example to perform measurements of RSSI for exposure notification
 *
 *
 *******************************************************************************
 *******************************************************************************
 *******************************************************************************
*/
/* Bluetooth stack headers */
#include "bg_types.h"
#include "native_gecko.h"
#include "gatt_db.h"

#include "infrastructure.h"

#include "app.h"

#include "em_rtcc.h"
#include "em_core.h"

#include "spiflash/btl_storage.h"

#include "crypto/monocypher.h"
#include "crypto/x25519-cortex-m4.h"/*
 *
#include "retargetserial.h"

#if defined(HAL_CONFIG)
#include "retargetserialhalconfig.h"
#else
#include "retargetserialconfig.h"
#endif
*/

#define GOT_CHAR
#define ENCOUNTER

#ifdef ENCOUNTER

#include "encounter/encounter.h"
Encounter_record encounters[IDX_MASK+1];
uint32_t c_fifo_last_idx;
uint32_t p_fifo_last_idx;
#define		uart_printf		printLog
#define		printk			printLog
#endif

/* Print boot message */
static void bootMessage(struct gecko_msg_system_boot_evt_t *bootevt);
int test_encrypt(uint8_t *answer);
void test_encrypt_compare(void);

/* Flag for indicating DFU Reset must be performed */
static uint8_t boot_to_dfu = 0;

static bool output_serial = false;
static bool write_flash = false;
enum
{
  HANDLE_DEMO, HANDLE_IBEACON
};
enum
{
  HANDLE_ADV, HANDLE_UART
};

struct
{
  uint8_t flagsLen; /* Length of the Flags field. */
  uint8_t flagsType; /* Type of the Flags field. */
  uint8_t flags; /* Flags field. */
  uint8_t uuid16Len; /* Length of the Manufacturer Data field. */
  uint8_t uuid16Type;
  uint8_t uuid16Data[2]; /* Type of the Manufacturer Data field. */
  uint8_t svcLen; /* Company ID field. */
  uint8_t svcType; /* Company ID field. */
  uint8_t svcID[2]; /* Beacon Type field. */
  uint8_t uuid[16]; /* 128-bit Universally Unique Identifier (UUID). The UUID is an identifier for the company using the beacon*/
  uint8_t meta[4]; /* Beacon Type field. */
} etAdvData = {
/* Flag bits - See Bluetooth 4.0 Core Specification , Volume 3, Appendix C, 18.1 for more details on flags. */
2, /* length  */
0x01, /* type */
0x04 | 0x02, /* Flags: LE General Discoverable Mode, BR/EDR is disabled. */

/* Manufacturer specific data */
3, /* length of field*/
0x03, /* type */
{0x6F, 0xFD},

0x17, /* Length of service data */
0x16, /* Type */
{0x6F, 0xFD},  /* EN service */

/* 128 bit / 16 byte UUID */
{0xC0, 0x19, 0x01, 0x00,
 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00,
 0x00, 0x02, 0x19, 0xC0},

 {0x40, 0, 0, 0}

};

void newRandAddress(void) {
  bd_addr rnd_addr;
  struct gecko_msg_le_gap_set_advertise_random_address_rsp_t *response;
  // struct gecko_msg_le_gap_stop_advertising_rsp_t *stop_response;
  //struct gecko_msg_le_gap_set_advertise_configuration_rsp_t *config_response;
  struct gecko_msg_system_get_random_data_rsp_t *rnd_response;

  //stop_response =
  gecko_cmd_le_gap_stop_advertising(HANDLE_IBEACON);
  // printLog("stop response: %d\r\n", stop_response->result);
  // config_response = gecko_cmd_le_gap_set_advertise_configuration(HANDLE_IBEACON, 0x04);
  // printLog("config response: %d\r\n", config_response->result);

  do {
	  rnd_response = gecko_cmd_system_get_random_data(6);
	  // printLog("rnd response: %d %d\r\n", rnd_response->result, rnd_response->data.len);
	  rnd_response->data.data[0] &= 0xFC;  // Last two bits must 0 for valid random mac address
	  memcpy(rnd_addr.addr, rnd_response->data.data, 6);
	  /*
	  printLog("new    board_addr    :   %02x:%02x:%02x:%02x:%02x:%02x\r\n",
	  rnd_addr.addr[5],
	  rnd_addr.addr[4],
	  rnd_addr.addr[3],
	  rnd_addr.addr[2],
	  rnd_addr.addr[1],
	  rnd_addr.addr[0]
					);
					*/
	  response = gecko_cmd_le_gap_set_advertise_random_address(HANDLE_IBEACON, 0x03, rnd_addr);
  } while(response->result>0);
  /*
   printLog("random board_addr %3d:   %02x:%02x:%02x:%02x:%02x:%02x\r\n", response->result,
  response->address_out.addr[5],
  response->address_out.addr[4],
  response->address_out.addr[3],
  response->address_out.addr[2],
  response->address_out.addr[1],
  response->address_out.addr[0]
  );
  */

  gecko_cmd_le_gap_start_advertising(HANDLE_IBEACON, le_gap_user_data, le_gap_non_connectable);
}
void bcnSetupAdvBeaconing(void)
{

  /* This function sets up a custom advertisement package according to iBeacon specifications.
   * The advertisement package is 30 bytes long. See the iBeacon specification for further details.
   */

  //
  //uint8_t len = sizeof(bcnBeaconAdvData);
  //uint8_t *pData = (uint8_t*)(&bcnBeaconAdvData);
  uint8_t len = sizeof(etAdvData);
  uint8_t *pData = (uint8_t*)(&etAdvData);
  // bd_addr rnd_addr;
  // struct gecko_msg_le_gap_set_advertise_random_address_rsp_t *response;
  /* Set custom advertising data */

  gecko_cmd_le_gap_bt5_set_adv_data(HANDLE_IBEACON, 0, len, pData);

  /* Set 0 dBm Transmit Power */
  gecko_cmd_le_gap_set_advertise_tx_power(HANDLE_IBEACON, 0);
  gecko_cmd_le_gap_set_advertise_configuration(HANDLE_IBEACON, 0x04);
  /*
   *
  rnd_addr.addr[0] = 1;
  rnd_addr.addr[1] = 2;
  rnd_addr.addr[2] = 3;
  rnd_addr.addr[3] = 4;
  rnd_addr.addr[4] = 5;
  rnd_addr.addr[5] = 6;

  response = gecko_cmd_le_gap_set_advertise_random_address(HANDLE_IBEACON, 0x03, rnd_addr);

  printLog("board_addr:   %02x:%02x:%02x:%02x:%02x:%02x\r\n",
  rnd_addr.addr[5],
  rnd_addr.addr[4],
  rnd_addr.addr[3],
  rnd_addr.addr[2],
  rnd_addr.addr[1],
  rnd_addr.addr[0]
  );

  printLog("random board_addr %d:   %02x:%02x:%02x:%02x:%02x:%02x\r\n", response->result,
  response->address_out.addr[5],
  response->address_out.addr[4],
  response->address_out.addr[3],
  response->address_out.addr[2],
  response->address_out.addr[1],
  response->address_out.addr[0]
  );
  */

  /* Set advertising parameters. 200ms (320/1.6) advertisement interval. All channels used.
   * The first two parameters are minimum and maximum advertising interval, both in
   * units of (milliseconds * 1.6).  */
  gecko_cmd_le_gap_set_advertise_timing(HANDLE_IBEACON, 320, 320, 0, 0);

  /* Start advertising in user mode */
  gecko_cmd_le_gap_start_advertising(HANDLE_IBEACON, le_gap_user_data, le_gap_non_connectable);
  gecko_cmd_hardware_set_soft_timer(32768*10,HANDLE_ADV,0);
  //gecko_cmd_hardware_set_soft_timer(32768>>2,HANDLE_UART,0);

}
uint8_t private_key[32];
uint8_t public_key[32];
uint8_t shared_key[32];

void update_public_key(void) {
	struct gecko_msg_system_get_random_data_rsp_t *rnd_response;
	rnd_response = gecko_cmd_system_get_random_data(16);
	memcpy(private_key, rnd_response->data.data, 16);
	rnd_response = gecko_cmd_system_get_random_data(16);
	memcpy(private_key+16, rnd_response->data.data, 16);

	X25519_calc_public_key(public_key, private_key);
}

void uart_ch_rssi(struct Ch_data e) {
    uart_printf("\t%d %d %d %d %d\r\n", e.n, e.max, e.min, e.mean, e.var);

}

int in_encounters_fifo(const uint8_t * mac, uint32_t epoch_minute) {
    /*
     *
     * Search backward during the "last minutes" worth of encounters
     *
     */
    Encounter_record *current_encounter;
    if (c_fifo_last_idx == 0) return -1;
    int start = c_fifo_last_idx - 1;
    do {
        current_encounter = encounters + (start & IDX_MASK);
        if (current_encounter->minute < epoch_minute) return -1;
        if (memcmp(current_encounter->mac, mac, 6) == 0) return start;
        start--;
    } while (start>=0);
    return -1;
}

void startObserving(uint16_t interval, uint16_t window) {
	gecko_cmd_le_gap_end_procedure();
	/*  Start observing
	 * Use  extended to get channel info as well
	 */
	gecko_cmd_le_gap_set_discovery_extended_scan_response(1);
	/* scan on 1M PHY at 200 ms scan window/interval*/
	gecko_cmd_le_gap_set_discovery_timing(le_gap_phy_1m, interval, window);
	/* passive scanning */
	gecko_cmd_le_gap_set_discovery_type(le_gap_phy_1m,0);
	/* discover all devices on 1M PHY*/
	gecko_cmd_le_gap_start_discovery(le_gap_phy_1m,le_gap_discover_observation);
}

#define TICKS_PER_SECOND 32768
uint32_t time_overflow=0;
uint32_t epochtime = 0;
uint32_t offset_time = 0;
uint32_t offset_overflow = 0;
uint32_t next_minute = 60000;
uint32_t offsettime = 0;
uint32_t epochtimesync = 0;

uint32_t ts_ms() {
	/*  Need to handle overflow... This will happen every 36 hours...
	 *
	 */
	static uint32_t old_ts = 0;
	uint32_t ts = RTCC_CounterGet();
	uint32_t t = ts/TICKS_PER_SECOND;
	uint32_t fraction = ts % TICKS_PER_SECOND;
	uint32_t t_ms = t*1000 + (1000*fraction)/TICKS_PER_SECOND;
    if (old_ts > ts) {
    	time_overflow++;
    }
    old_ts = ts;

	return t_ms;
}

uint32_t k_uptime_get() {
	return ts_ms();
}

uint8_t findServiceInAdvertisement(uint8_t *data, uint8_t len)
{
  uint8_t adFieldLength;
  uint8_t adFieldType;
  uint8_t i = 0;
  // Parse advertisement packet
  while ((i+3) < len) {
    adFieldLength = data[i];
    adFieldType = data[i + 1];
    if (adFieldType == 0x03) {
      // Look for exposure notification
      if (data[i + 2]==0x6F && data[i+3]==0xFD) {
        return 1;
      }
    }
    // advance to the next AD struct
    i += adFieldLength + 1;  // Need +1 because FieldLength byte is not included in length
  }
  return 0;
}
void get_uint32(uint32_t *number) {
	uint8_t *ptr;
	ptr = (uint8_t *)number;
	for (int count=0; count<4; count++) {
		ptr[count] = getchar() & 0xFF;
	}
}

void send_uint32(uint32_t number) {
	uint8_t *ptr;
	ptr = (uint8_t *) &number;
	for (int txCount = 0; txCount < 4; txCount++) {
		RETARGET_WriteChar(*ptr++);
	}
}
void send32bytes(uint8_t *buffer) {
	uint8_t *ptr;
	ptr = buffer;
	for (int txCount = 0; txCount < 32; txCount++) {
		RETARGET_WriteChar(*ptr++);
	}
}
uint32_t encounter_count = 0;

void determine_counts() {
	uint32_t count=0;
	bool empty=true;
	do {
		empty = verifyErased(count<<5, 32);
		if (!empty) count++;
	} while (!empty);
	encounter_count = count;
	printLog("number of records: %ld\r\n", count);
	gecko_cmd_gatt_server_write_attribute_value(gattdb_count, 0, 4, (const uint8*) &count);
}
void test_write_flash() {
	uint32_t addr=0;
	uint8_t buffer[32];
	for(int i=0; i<32; i++) {
		buffer[i] = 32-i;
	}
	int32_t retCode = storage_writeRaw(addr, buffer, 32);
	printLog("storage_writeRaw: %ld\r\n", retCode);
    send32bytes(buffer);
}
void store_event(uint8_t *event) {
    CORE_DECLARE_IRQ_STATE;

    CORE_ENTER_ATOMIC();

	if (encounter_count<(1<<(20-5))) {
		int32_t retCode = storage_writeRaw(encounter_count<<5, event, 32);
		if (retCode) {
			printLog("Error storing to flash %ld\r\n", retCode);
		}
		encounter_count++;
		printLog("Encounter_count: %ld\r\n", encounter_count);
		gecko_cmd_gatt_server_write_attribute_value(gattdb_count, 0, 4, (const uint8*) &encounter_count);
	} else {
		/* Need to indicate that memory is full */
		;
	}
    CORE_EXIT_ATOMIC(); // Enable interrupts
}


const char *version_str = "Version: " __DATE__ " " __TIME__;
void parse_command(char c) {
	switch(c) {
	case 'r':
		output_serial = true;
		break;
	case 'z':
		output_serial = false;
		break;
	case 'g':{
		uint32_t addr=0;
		uint8_t buffer[32];
		int bad=0;
		for (int count=0; count<encounter_count; count++) {
			int32_t retCode = storage_readRaw(addr, buffer, 32);
			if (retCode) {
				bad++;
			}
			// printLog("storage_readRaw: %ld\r\n", retCode);
			send32bytes(buffer);
			addr += 32;
		}
		printLog("bad: %d\r\n", bad);
        break;
	}
	case 'G':{
		uint32_t addr=0;
		uint8_t buffer[32];
		get_uint32(&addr);
		int bad=0;
			int32_t retCode = storage_readRaw(addr<<5, buffer, 32);
			if (retCode) {
				bad++;
			}
			// printLog("storage_readRaw: %ld\r\n", retCode);
			send32bytes(buffer);

		// printLog("bad: %d\r\n", bad);
        break;
	}
	case 'w':{
		write_flash = true;
		printLog("Start writing to flash\r\n");
        break;
	}
	case 's':{
		write_flash = false;
		printLog("Stop writing to flash\r\n");
        break;
	}
	case 'a': {
		uint32_t ts = ts_ms();
		send_uint32(ts);
		send_uint32(time_overflow);

		break;
	}
	case 'o': {
		uint8_t time_evt[32];
        get_uint32(&epochtime);
        get_uint32(&offset_time);
        get_uint32(&offset_overflow);
		printLog("epochtime offset_times: %ld %ld %ld\r\n", epochtime, offset_time, offset_overflow);
		memset(time_evt, 0, 32);
		memcpy(time_evt+4, &epochtime, 4);
		memcpy(time_evt+8, &offset_time, 4);
		memcpy(time_evt+12, &offset_overflow, 4);
	    store_event(time_evt);

		break;
	}
    case 'h':
        printLog("%s\r\n", version_str);
        break;

	default:
		break;
	}

}

void process_scan(struct gecko_cmd_packet* evt) {
	if(findServiceInAdvertisement(evt->data.evt_le_gap_extended_scan_response.data.data,
									evt->data.evt_le_gap_extended_scan_response.data.len)) {

		uint8_t channel = evt->data.evt_le_gap_extended_scan_response.channel;
		int8_t rssi = evt->data.evt_le_gap_extended_scan_response.rssi;
		// int adv_len = evt->data.evt_le_gap_extended_scan_response.data.len;
		uint32_t timestamp = ts_ms();
		uint8_t output[32];
		uint8_t *ptr;

		ptr = output;
		memset(ptr, 0, 32);
		memcpy(ptr, (uint8_t *) &timestamp, 4);
		memcpy(ptr+4, evt->data.evt_le_gap_extended_scan_response.address.addr, 6);
		ptr[10] = rssi;
		ptr[11] = channel;
		memcpy(ptr+12, evt->data.evt_le_gap_extended_scan_response.data.data+11, 20);
		/*
		printLog("t: %lu rssi: %d channel %d adv_len: %d\r\n", timestamp, rssi, channel, adv_len);
		printLog("received from:   %02x:%02x:%02x:%02x:%02x:%02x\r\n",
			evt->data.evt_le_gap_extended_scan_response.address.addr[5],
			evt->data.evt_le_gap_extended_scan_response.address.addr[4],
			evt->data.evt_le_gap_extended_scan_response.address.addr[3],
			evt->data.evt_le_gap_extended_scan_response.address.addr[2],
			evt->data.evt_le_gap_extended_scan_response.address.addr[1],
			evt->data.evt_le_gap_extended_scan_response.address.addr[0]);
		*/
		if (output_serial) {
			ptr = output;
			for (int txCount = 0; txCount < 32; txCount++) {
				RETARGET_WriteChar(*ptr++);
			}
		}
		if (write_flash) {
			store_event(output);
		}

	}
}

/*
 *
static void scan_cb_orig(const bt_addr_le_t *addr, s8_t rssi, u8_t adv_type,
            struct net_buf_simple *buf)
*/
// #define SCAN_DEBUG
void process_scan_encounter(struct gecko_cmd_packet* evt) {
	uint32_t timestamp = ts_ms();

#ifdef SCAN_DEBUG
    printk("%lu\tIn scan callback\n", timestamp);
#endif
	uint8_t channel = evt->data.evt_le_gap_extended_scan_response.channel;
	int saewoo_hack = (channel - 37) % 3; // Make sure it is less than 3 just in case
	int8_t rssi = evt->data.evt_le_gap_extended_scan_response.rssi;
	// int adv_len = evt->data.evt_le_gap_extended_scan_response.data.len;
	// uint8_t output[32];
	// uint8_t *ptr;
	uint8_t *mac_addr;

	mac_addr = evt->data.evt_le_gap_extended_scan_response.address.addr;
    rssi = -rssi;

    if (true) {
	/*
	if(findServiceInAdvertisement(evt->data.evt_le_gap_extended_scan_response.data.data,
									evt->data.evt_le_gap_extended_scan_response.data.len)) {
    */
#ifdef SCAN_DEBUG
	bool found_evt = findServiceInAdvertisement(evt->data.evt_le_gap_extended_scan_response.data.data,
									evt->data.evt_le_gap_extended_scan_response.data.len);
    printk("\tfinish parse data %d\r\n", found_evt);
#endif
        Encounter_record *current_encounter;
        if (timestamp>next_minute) next_minute+=60000;
        int sec_timestamp = (timestamp - (next_minute - 60000)) / 1000;
        uint32_t epoch_minute = ((timestamp-offsettime) / 1000 + epochtimesync)/60;

#ifdef SCAN_DEBUG
    printk("\tTry to check if in fifo, epoch_minute: %ld sec:%d\r\n", epoch_minute, sec_timestamp);
#endif
        // Check if record already exists by mac
        int idx = in_encounters_fifo(mac_addr, epoch_minute);
#ifdef SCAN_DEBUG
    printk("\tp_idx: %ld, c_idx: %ld\n", p_fifo_last_idx, c_fifo_last_idx);
#endif
        // uart_printf("idx:%d  ", idx);
        if (idx<0) {
            // No index returned
#ifdef SCAN_DEBUG
    printk("\tCreate new encounter: mask_idx: %ld\n", c_fifo_last_idx & IDX_MASK);
#endif
            current_encounter = encounters + (c_fifo_last_idx & IDX_MASK);
            memset((uint8_t *)current_encounter, 0, 64);  // clear all values
            memcpy(current_encounter->mac, mac_addr, 6);
            current_encounter->rssi_data[saewoo_hack].n = 1;
            current_encounter->rssi_data[saewoo_hack].max = rssi;
            current_encounter->rssi_data[saewoo_hack].min = rssi;
            current_encounter->rssi_data[saewoo_hack].mean = rssi;
            current_encounter->rssi_data[saewoo_hack].var = 0;
            current_encounter->first_time = sec_timestamp;
            current_encounter->last_time = sec_timestamp;
            current_encounter->minute = epoch_minute;
            c_fifo_last_idx++;
        } else {
#ifdef SCAN_DEBUG
    printk("\tFound previous encounter: %d, mask: %d\n",idx, (idx&IDX_MASK));
#endif
            current_encounter = encounters + (idx & IDX_MASK);
#ifdef SCAN_DEBUG
    printk("\tsaewoo_hack:%d\n",saewoo_hack );
#endif
            int old_n = current_encounter->rssi_data[saewoo_hack].n;
            int n = old_n + 1;
            current_encounter->rssi_data[saewoo_hack].n = n;
            if (rssi > current_encounter->rssi_data[saewoo_hack].max) {
                current_encounter->rssi_data[saewoo_hack].max = rssi;
            }
            if (rssi < current_encounter->rssi_data[saewoo_hack].max) {
                current_encounter->rssi_data[saewoo_hack].min = rssi;
            }
#ifdef SCAN_DEBUG
    printk("\tcompute new mean:old_n, n:%d, %d\n",old_n, n );
#endif
            if (n>0) {
                int mean = current_encounter->rssi_data[saewoo_hack].mean;
                int new_mean = mean + (rssi - mean + n/2)/n;
                current_encounter->rssi_data[saewoo_hack].mean = new_mean;
                current_encounter->rssi_data[saewoo_hack].var += (rssi-new_mean)*(rssi-mean);
            }  // otherwise, 255 interactions in 1 minute... It has rolled over... don't computer new stuff
            current_encounter->last_time = sec_timestamp;
            current_encounter->minute = epoch_minute;
        }
#ifdef SCAN_DEBUG
        uart_ch_rssi(current_encounter->rssi_data[saewoo_hack]);
#endif
        // Update bobs shared key
#ifdef SCAN_DEBUG
    printk("\tDo crypto stuff\n");
#endif
        uint8_t hi_lo_byte = evt->data.evt_le_gap_extended_scan_response.data.data[30];
        uint8_t *rpi;
        rpi = evt->data.evt_le_gap_extended_scan_response.data.data + 11;
        // uart_printf("\ttime: %u, flag: %d, hi_lo_byte: %d\n", timestamp, current_encounter->flag, hi_lo_byte);
        if (( current_encounter->flag &0x3) < 3) {
            if (((current_encounter->flag & 0x1) == 0) && (hi_lo_byte==0)) {
#ifdef SCAN_DEBUG
                uart_printf("\tgot lo part of public key\n");
#endif
                memcpy(current_encounter->public_key, rpi, 16);
                current_encounter->flag |= 0x1;
            }
            if (((current_encounter->flag & 0x2) == 0) && (hi_lo_byte==1)) {
#ifdef SCAN_DEBUG
                uart_printf("\tgot hi part of public key\n");
#endif
                memcpy(current_encounter->public_key+16, rpi, 16);
                current_encounter->flag |= 0x2;
            }
        }
        if (((current_encounter->flag&0x4)==0) && ((current_encounter->flag&0x3) == 3)) {
#ifdef SCAN_DEBUG
            uart_printf("\tcalc shared key\n");
#endif
            X25519_calc_shared_secret(shared_key, private_key, current_encounter->public_key);
            memcpy(current_encounter->public_key, shared_key, 32);
            current_encounter->flag |= 0x4;

        }
        memset(current_encounter->public_key, 0xFF, 32);
#ifdef SCAN_DEBUG
        printk("\tdone scan cb\n");
#endif
        // uart_encounter(*current_encounter);
    }
}

void flash_store(void) {
    Encounter_record *current_encounter;
    // uint32_t start = p_fifo_last_idx;
    uint32_t timestamp = ts_ms();
    uint32_t epoch_minute = ((timestamp-offsettime) / 1000 + epochtimesync)/60;
    /*
    printLog("flash_store, epoch_minute: %ld, p_idx: %ld, c_idx: %ld\n", epoch_minute,
                    p_fifo_last_idx, c_fifo_last_idx);
	*/
    while (c_fifo_last_idx > p_fifo_last_idx) {

        current_encounter = encounters + (p_fifo_last_idx & IDX_MASK);
        // uart_printf("idx: %d, minute: %d\n", p_fifo_last_idx, current_encounter->minute);
        if (current_encounter->minute < epoch_minute) { // this is an old record write to flash
        	uint8_t *ptr;
        	ptr = (uint8_t *) current_encounter;
        	send32bytes(ptr);
        	send32bytes(ptr+32);
        	/*
        	store_event(ptr);
        	store_event(ptr+32);
        	*/
            p_fifo_last_idx++;
        } else {
            // uart_printf("Done flash_store looking back\n");
            return;
        }
    }
    // uart_printf("Done flash_store, after while\n");

}

/* Main application */
void appMain(gecko_configuration_t *pconfig)
{
#if DISABLE_SLEEP > 0
  pconfig->sleep.flags = 0;
#endif

  /* Initialize debug prints. Note: debug prints are off by default. See DEBUG_LEVEL in app.h */
  initLog();

  /* Initialize stack */
  gecko_init(pconfig);
  int32_t flash_ret = storage_init();
  uint32_t flash_size = storage_size();
  printLog("storage_init: %ld %ld\r\n", flash_ret, flash_size);
  determine_counts();
  test_encrypt_compare();

  while (1) {
    /* Event pointer for handling events */
    struct gecko_cmd_packet* evt;

    /* if there are no events pending then the next call to gecko_wait_event() may cause
     * device go to deep sleep. Make sure that debug prints are flushed before going to sleep */
    if (!gecko_event_pending()) {
      flushLog();
    }

     //int c = RETARGET_ReadChar();
    int c = getchar();
    // printLog("ReadChar: %d\r\n", c);
    if (c>0) {
		#ifdef GOT_CHAR
    	printLog("got %c\r\n", c);
		#endif
    	parse_command(c);
    	gecko_cmd_gatt_server_write_attribute_value(gattdb_Read_Write, 0, 1, (const uint8*) &c);
    }
    // RETARGET_SerialFlush();

    /* Check for stack event. This is a blocking event listener. If you want non-blocking please see UG136. */

    evt = gecko_wait_event();

    // struct gecko_cmd_packet* evt;
    // CORE_DECLARE_IRQ_STATE;
    /* Poll (non-blocking) for a Bluetooth stack event. */
    // evt = gecko_peek_event();
    /* Run application and event handler. */
    if(evt != NULL){

		/* Handle events */
		//printLog("event header: %ld\r\n", evt->header);
		switch (BGLIB_MSG_ID(evt->header)) {
		  /* This boot event is generated when the system boots up after reset.
		   * Do not call any stack commands before receiving the boot event.
		   * Here the system is set to start advertising immediately after boot procedure. */
		  case gecko_evt_system_boot_id:

			bootMessage(&(evt->data.evt_system_boot));
			printLog("boot event - starting advertising\r\n");

			/* Set advertising parameters. 100ms advertisement interval.
			 * The first parameter is advertising set handle
			 * The next two parameters are minimum and maximum advertising interval, both in
			 * units of (milliseconds * 1.6).
			 * The last two parameters are duration and maxevents left as default. */
			gecko_cmd_le_gap_set_advertise_timing(HANDLE_DEMO, 320, 320, 0, 0);

			gecko_cmd_le_gap_set_advertise_tx_power(HANDLE_DEMO, 0);

			/* Start general advertising and enable connections. */
			gecko_cmd_le_gap_start_advertising(0, le_gap_general_discoverable, le_gap_connectable_scannable);
			/*  Start advertising non-connectable packets */
			bcnSetupAdvBeaconing();
			startObserving(320, 320);

			break;

		  case gecko_evt_le_gap_extended_scan_response_id: {
			  // printLog("scan event\r\n");
#ifdef ENCOUNTER
			  process_scan_encounter(evt);
#else
			  process_scan(evt);
#endif
			break;
		  }
		  case gecko_evt_hardware_soft_timer_id:
		  {
			  uint32_t ts = ts_ms();
			  if (ts>next_minute) next_minute+= 60000;

			  switch(evt->data.evt_hardware_soft_timer.handle) {
			  case HANDLE_ADV:
				  newRandAddress();
				  flash_store();
				  break;
			  case HANDLE_UART:
			  {
				  int c = getchar();
				  // printLog("Handle UART %d\r\n", c);
				  if (c>0) {
						#ifdef GOT_CHAR
				    	printLog("%ld: got ord:%d char:%c\r\n", ts, c&0xFF,c);
						#endif
				    	parse_command(c);
				    	gecko_cmd_gatt_server_write_attribute_value(gattdb_Read_Write, 0, 1, (const uint8*) &c);
				  }
				  break;
			  }
			  default:
				  break;
			  }
			  break;
		  }
		  case gecko_evt_le_connection_opened_id:

			printLog("connection opened\r\n");

			break;

		  case gecko_evt_le_connection_closed_id:

			printLog("connection closed, reason: 0x%2.2x\r\n", evt->data.evt_le_connection_closed.reason);

			/* Check if need to boot to OTA DFU mode */
			if (boot_to_dfu) {
			  /* Enter to OTA DFU mode */
			  gecko_cmd_system_reset(2);
			} else {
			  /* Restart advertising after client has disconnected */
			  gecko_cmd_le_gap_start_advertising(0, le_gap_general_discoverable, le_gap_connectable_scannable);
			}
			break;
		  case gecko_evt_gatt_server_attribute_value_id:
		  {
			  if (evt->data.evt_gatt_server_attribute_value.attribute==gattdb_Read_Write) {
				  int c = evt->data.evt_gatt_server_attribute_value.value.data[0];
				  parse_command(c);
				  // printLog("new attribute value %c\r\n", c);
			  }

			  break;
		  }
		  /* Events related to OTA upgrading
			 ----------------------------------------------------------------------------- */

		  /* Check if the user-type OTA Control Characteristic was written.
		   * If ota_control was written, boot the device into Device Firmware Upgrade (DFU) mode. */
		  case gecko_evt_gatt_server_user_write_request_id:
			printLog("user_write_request\r\n");
			if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_ota_control) {
			  /* Set flag to enter to OTA mode */
			  boot_to_dfu = 1;
			  /* Send response to Write Request */
			  gecko_cmd_gatt_server_send_user_write_response(
				evt->data.evt_gatt_server_user_write_request.connection,
				gattdb_ota_control,
				bg_err_success);

			  /* Close connection to enter to DFU OTA mode */
			  gecko_cmd_le_connection_close(evt->data.evt_gatt_server_user_write_request.connection);
			}
			break;

		  /* Add additional event handlers as your application requires */

		  default:
			break;
		}
    }
    //CORE_ENTER_ATOMIC();
    // Disable interrupts
    /* Check how long the stack can sleep */
    //uint32_t durationMs = gecko_can_sleep_ms();
    /* Go to sleep. Sleeping will be avoided if there isn't enough time to sleep */
    //gecko_sleep_for_ms(durationMs);
    //CORE_EXIT_ATOMIC(); // Enable interrupts
    // printLog("durationMs: %d\r\n", durationMs);
  }
}

/* Print stack version and local Bluetooth address as boot message */
static void bootMessage(struct gecko_msg_system_boot_evt_t *bootevt)
{
#if DEBUG_LEVEL
  bd_addr local_addr;
  int i;

  printLog("stack version: %u.%u.%u\r\n", bootevt->major, bootevt->minor, bootevt->patch);
  local_addr = gecko_cmd_system_get_bt_address()->address;

  printLog("local BT device address: ");
  for (i = 0; i < 5; i++) {
    printLog("%2.2x:", local_addr.addr[5 - i]);
  }
  printLog("%2.2x\r\n", local_addr.addr[0]);
#endif
}


uint8_t key_exchange_0[] = { 0xa5, 0x46, 0xe3, 0x6b, 0xf0, 0x52, 0x7c, 0x9d, 0x3b, 0x16, 0x15, 0x4b, 0x82, 0x46, 0x5e, 0xdd, 0x62, 0x14, 0x4c, 0x0a, 0xc1, 0xfc, 0x5a, 0x18, 0x50, 0x6a, 0x22, 0x44, 0xba, 0x44, 0x9a, 0xc4, };
#define key_exchange_0_size 32
uint8_t key_exchange_1[] = { 0xe6, 0xdb, 0x68, 0x67, 0x58, 0x30, 0x30, 0xdb, 0x35, 0x94, 0xc1, 0xa4, 0x24, 0xb1, 0x5f, 0x7c, 0x72, 0x66, 0x24, 0xec, 0x26, 0xb3, 0x35, 0x3b, 0x10, 0xa9, 0x03, 0xa6, 0xd0, 0xab, 0x1c, 0x4c, };
#define key_exchange_1_size 32

int test_encrypt(uint8_t *answer) {
    uint8_t       shared_key[32];
    uint32_t start = k_uptime_get();

    int res=0;

    for (int i=0; i<100; i++) {
        X25519_calc_shared_secret(shared_key, key_exchange_0, key_exchange_1);
        // crypto_x25519(shared_key, key_exchange_0, key_exchange_1);
        // crypto_key_exchange(shared_key, key_exchange_0, key_exchange_1);
        res += memcmp(shared_key, answer, 32);
    }
    if (res!=0) {
        return -1; }
    uint32_t stop = k_uptime_get();
    return stop-start;
}
void test_encrypt_compare(void)
{
    int res = 0;
    uint8_t shared_key[32];
    uint8_t shared_key2[32];
    uint8_t public[64];
    memset(public, 0, 64);

    printLog("Hello World! testing crypto\r\n");
    memcpy(public, key_exchange_1, 32);
    X25519_calc_shared_secret(shared_key, key_exchange_0, public);

    crypto_x25519(shared_key2, key_exchange_0, key_exchange_1);
    res += memcmp(shared_key, shared_key2, 32);
    printLog("res: %d\r\n", res);

    if (true) {
        res = test_encrypt(shared_key);
    }
    printLog("time: %d\n", res);


}
