 /*******************************************************************************
 *
 * Copyright 2020 Sae Woo Nam
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *
 * Modified Silabs empty example to perform measurements of RSSI for exposure notification
 *
 *
*/

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
#include "crypto/x25519-cortex-m4.h"
#include "sleep.h"

/* button */
#include "em_gpio.h"
#include "gpiointerrupt.h"
/* battery */
#include "em_device.h"
#include "em_cmu.h"
#include "em_iadc.h"

#define THREE_BUTTONS
/* Definitions of external signals */
#define BUTTON_PRESSED (1 << 0)
#define BUTTON_RELEASED (1 << 1)
#define LOG_MARK (1<<2)
#define LOG_UNMARK (1<<3)
#define LOG_RESET (1<<4)
/*
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
uint32_t c_fifo_last_idx=0;
uint32_t p_fifo_last_idx=0;
#define		uart_printf		printLog
#define		printk			printLog
#endif


/***************************************************************************************************
  Local Macros and Definitions for handling OTA file/data xfer
 **************************************************************************************************/

#define STATE_ADVERTISING 1
#define STATE_CONNECTED   2
#define STATE_SPP_MODE    3


/***************************************************************************************************
 Local Variables for OTOA file/data xfer
 **************************************************************************************************/
static uint8 _conn_handle = 0xFF;
static int _main_state;
static uint8_t battBatteryLevel; /* Battery Level */

typedef struct
{
	uint32_t num_pack_sent;
	uint32_t num_bytes_sent;
	uint32_t num_pack_received;
	uint32_t num_bytes_received;
	uint32_t num_writes; /* Total number of send attempts */
	uint32_t bad_mem_reads;
} tsCounters;

tsCounters _sCounters;

static uint8 _max_packet_size = 20; // Maximum bytes per one packet
static uint8 _min_packet_size = 20; // Target minimum bytes for one packet

static void reset_variables()
{
	_conn_handle = 0xFF;
	_main_state = STATE_ADVERTISING;

	_max_packet_size = 20;

	memset(&_sCounters, 0, sizeof(_sCounters));
}

void printStats(tsCounters *psCounters)
{
	printLog("Outgoing data:\r\n");
	printLog(" bytes/packets sent: %lu / %lu ", psCounters->num_bytes_sent, psCounters->num_pack_sent);
	printLog(", num writes: %lu\r\n", psCounters->num_writes);
#ifdef RX_OVERFLOW_TRACKING
	if(rxOverFlow) {
		printLog(" NOTE: RX buffer overflowed %d times\r\n", rxOverFlow);
	} else {
		printLog(" No RX buffer overflow detected\r\n");
	}
#else
	printLog("(RX buffer overflow is not tracked)\r\n");
#endif

	printLog("Incoming data:\r\n");
	printLog(" bytes/packets received: %lu / %lu\r\n", psCounters->num_bytes_received, psCounters->num_pack_received);

	return;
}
/**************************
 * End of file xfer stuff
 */


/* Print boot message */
static void bootMessage(struct gecko_msg_system_boot_evt_t *bootevt);
static void adcInit(void);
void readBatteryLevel(void);

int test_encrypt(uint8_t *answer);
void test_encrypt_compare(void);
void update_public_key(void);
void flash_store(void);

/* Flag for indicating DFU Reset must be performed */
static uint8_t boot_to_dfu = 0;

static bool output_serial = false;
static bool write_flash = false;
static bool sending_ota = false;
static bool sending_turbo = false;
static bool mark_set = false;
static int clicks = 0;
static int flashes = 0;

uint32_t encounter_count = 0;
uint32_t untransferred_start = 0;

uint8_t private_key[32];
uint8_t public_key[32];
uint8_t shared_key[32];

enum
{
  MODE_RAW, MODE_ENCOUNTER
};
enum
{
  HANDLE_DEMO, HANDLE_IBEACON
};
enum
{
  HANDLE_ADV, HANDLE_UART, HANDLE_UPDATE_KEY, HANDLE_LED, HANDLE_CLICK
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
  uint8_t key[16]; /* 128-bit Universally Unique Identifier (UUID). The UUID is an identifier for the company using the beacon*/
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

#define TICKS_PER_SECOND 32768
uint32_t time_overflow=0;   // current time overflow
uint32_t offset_time = 0;
uint32_t offset_overflow = 0;  // set of offset has overflowed
uint32_t next_minute = 60000;
uint32_t epochtimesync = 0;
struct
{
	uint32_t time_overflow;
	uint32_t offset_time;
	uint32_t offset_overflow;
	uint32_t next_minute;
	uint32_t epochtimesync;
	bool gottime;
} _time = {
		0, 0, 0, 60000, 0, false
};

#ifdef ENCOUNTER
int mode = MODE_ENCOUNTER;
#else
int mode = MODE_RAW;
#endif

uint32_t ts_ms() {
	/*  Need to handle overflow... of t_ms... 36hrs*32... perhaps every battery change
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
    t_ms += (1000*time_overflow) << 17; // RTCC overflows after 1<<17 seconds
    old_ts = ts;

	return t_ms;
}

void wait(int wait) {
	printLog("Start wait %d\r\n", wait);
	uint32_t start = ts_ms();
	while((ts_ms() - start)<wait) {
		;
	}
	printLog("Done wait %d\r\n", wait);

}
uint32_t k_uptime_get() {
	return ts_ms();
}

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
	  rnd_response->data.data[5] &= 0x3F;  // Last two bits must 0 for valid random mac address
	  // rnd_response->data.data[0] = 0xFF;  // Make it easier to debug

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
	  // printLog("set random address respose %ld, 0x%X\r\n", ts_ms(), response->result);
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
void update_adv_key(){
	  uint8_t len = sizeof(etAdvData);
	  uint8_t *pData = (uint8_t*)(&etAdvData);

	  if (etAdvData.meta[3] == 1) {
		  etAdvData.meta[3] = 0;  //Clear first bit
		  memcpy(etAdvData.key, public_key, 16);
	  } else {
		  etAdvData.meta[3] = 1;  //Set first bit 0
		  memcpy(etAdvData.key, public_key+16, 16);
	  }
	  gecko_cmd_le_gap_bt5_set_adv_data(HANDLE_IBEACON, 0, len, pData);
}
void bcnSetupAdvBeaconing(void)
{

  /* This function sets up a custom advertisement package according to iBeacon specifications.
   * The advertisement package is 30 bytes long. See the iBeacon specification for further details.
   */

  //
  // uint8_t len = sizeof(etAdvData);
  // uint8_t *pData = (uint8_t*)(&etAdvData);
  // bd_addr rnd_addr;
  // struct gecko_msg_le_gap_set_advertise_random_address_rsp_t *response;
  /* Set custom advertising data */
  update_public_key();
  update_adv_key();
  // gecko_cmd_le_gap_bt5_set_adv_data(HANDLE_IBEACON, 0, len, pData);

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
  // gecko_cmd_hardware_set_soft_timer(32768*60,HANDLE_ADV,0);
  gecko_cmd_hardware_set_soft_timer(32768,HANDLE_UPDATE_KEY,0);
  //gecko_cmd_hardware_set_soft_timer(32768>>2,HANDLE_UART,0);
  // gecko_cmd_hardware_set_soft_timer(32768,HANDLE_LED,0);

}

void update_public_key(void) {
	struct gecko_msg_system_get_random_data_rsp_t *rnd_response;
	rnd_response = gecko_cmd_system_get_random_data(16);
	memcpy(private_key, rnd_response->data.data, 16);
	rnd_response = gecko_cmd_system_get_random_data(16);
	memcpy(private_key+16, rnd_response->data.data, 16);

	X25519_calc_public_key(public_key, private_key);
}

uint32_t epoch_day(void) {
	uint32_t timestamp = ts_ms();
	return ((timestamp-offset_time) / 1000 + epochtimesync)/(60*60*24);
}

void update_next_minute(void) {
	static uint32_t day = 0;
	next_minute +=60000;
	//  Change key daily... for now.
	if (day==0) {
		day = epoch_day();
	}
	uint32_t current_day = epoch_day();
	if (current_day>day) {
		update_public_key();
		newRandAddress();
		day = current_day;
	}
	if (write_flash) flash_store();
}

void setup_next_minute(void) {
	printLog("setup next_minute\r\n");
    uint32_t timestamp = ts_ms();
    uint32_t epoch_minute_origin = (epochtimesync)/60;
    uint32_t extra_seconds = epochtimesync - 60*epoch_minute_origin;
    next_minute = offset_time - extra_seconds * TICKS_PER_SECOND + 60000;
    printLog("offset_time, timestamp, next minute: %ld, %ld, %ld\r\n", offset_time, timestamp, next_minute);
    /*
    while (next_minute < timestamp) {
    	next_minute += 60000;
    }
    */
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

void setConnectionTiming(uint16_t *params) {
	// Set connection parameters
	if (_conn_handle < 0xFF) {
		gecko_cmd_le_connection_set_timing_parameters(_conn_handle, params[0],
				params[1], 0, params[2], 0, 0xFFFF);
	}
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

void determine_counts(uint32_t flash_size) {
	uint32_t count=0;
	bool empty=true;
	// printf("flash_size<<5: %ld\r\n", (flash_size>>5));
	do {
		empty = verifyErased(count<<5, 32);
		if (!empty) count++;
		// printLog("count:%lu\r\n", count);
	} while (!empty & (count<(flash_size>>5)));

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
void flash_erase() {
	printLog("Trying to erase\r\n");
	int num_sectors = encounter_count >> 7;
	if (4095 & (encounter_count<<5)) num_sectors++;
	int32_t retCode = storage_eraseRaw(0, num_sectors<<12);
    printLog("erase size: %d\r\n", num_sectors<<12);
	// int32_t retCode = storage_eraseRaw(0, encounter_count<<5);
	// int32_t retCode = storage_eraseRaw(0, 1<<20);
	if (retCode) {
		printLog("Problem with erasing flash. %ld\r\n", retCode);
	} else {
		encounter_count = 0;
		gecko_cmd_gatt_server_write_attribute_value(gattdb_count, 0, 4, (const uint8*) &encounter_count);
		untransferred_start = 0;
	}
	// bool empty = verifyErased(0, 1<<20);
	// printLog("verify erased: %d\r\n", empty);
}
void store_event(uint8_t *event) {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

	if (encounter_count<(1<<(20-5))) {
		int32_t retCode = storage_writeRaw(encounter_count<<5, event, 32);
		/*for (int i=0; i<32; i++) {
			printLog("%02X ", event[i]);
		}
		printLog("\r\n");*/
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

void store_time(uint8_t *data, uint8_t len) {
    uint8_t time_evt[32];
	memset(time_evt, 0, 32);
	memcpy(time_evt+4, &epochtimesync, 4);
	memcpy(time_evt+8, &offset_time, 4);
	memcpy(time_evt+12, &offset_overflow, 4);
	memset(time_evt+16, 0xFF, 16);
	if (len>0) {
		if (len>16) len=16;
		memcpy(time_evt+16, data, len);
	}
	_time.gottime = true;
	_time.epochtimesync = epochtimesync;
	_time.next_minute = next_minute;
	_time.offset_overflow = offset_overflow;
	_time.offset_time = offset_time;

    store_event(time_evt);
}

void send_ota_msg(char *msg) {
	uint32_t len = strlen(msg);
	uint16 result;
    printLog("Try to send_ota_msg: %s\r\n", msg);

	do {
		result = gecko_cmd_gatt_server_send_characteristic_notification(_conn_handle, gattdb_gatt_spp_data, len, (uint8_t *) msg)->result;
	} while(result == bg_err_out_of_memory);

}
void send_ota_uint32(uint32_t data) {
	uint32_t len = 4;
	uint16 result;

	do {
		result = gecko_cmd_gatt_server_send_characteristic_notification(_conn_handle, gattdb_gatt_spp_data, len, (uint8_t *) &data)->result;
	} while(result == bg_err_out_of_memory);
}

void send_ota_uint8(uint8_t data) {
	uint32_t len = 1;
	uint16 result;

	do {
		result = gecko_cmd_gatt_server_send_characteristic_notification(_conn_handle, gattdb_gatt_spp_data, len, (uint8_t *) &data)->result;
	} while(result == bg_err_out_of_memory);
}

void send_chunk(uint32_t index) {
	uint32_t max;
	uint32_t len = encounter_count<<5;
	uint8 data[256];
	uint16 result;
	int chunk_size = _min_packet_size - 4;
    // check if index is in range
	// printLog("send_chunk index: %ld, max_chunk_size %d\r\n", index, chunk_size);
	max = len/chunk_size;
	if ((len%chunk_size)== 0) {
		max = max-1;
	}
	if (index > max) {
		// send over nothing because index is too high return nothing but the index
		memcpy(data, &index, 4);
		do {
			result = gecko_cmd_gatt_server_send_characteristic_notification(_conn_handle, gattdb_gatt_spp_data, 4, data)->result;
		} while(result == bg_err_out_of_memory);
	} else {
		// send over data... two cases... full chunk and partial chunk (240 bytes)
		memcpy(data, &index, 4);
		// figure out actual size of the packet to send... usually 240 except at the end
		int xfer_len = chunk_size;
		if (index==max) {
			// this is likely a partial
			xfer_len = len%chunk_size;
			if (xfer_len==0) xfer_len=chunk_size;
		}
		// Figure out starting point in memory
		uint32_t addr = chunk_size*index;
		do {
			xfer_len += 4;  // Add length of packet number
			int32_t retCode = storage_readRaw(addr, data+4, xfer_len);
			if (retCode) _sCounters.bad_mem_reads++;
			result = gecko_cmd_gatt_server_send_characteristic_notification(_conn_handle, gattdb_gatt_spp_data, xfer_len, data)->result;
			_sCounters.num_writes++;
		} while(result == bg_err_out_of_memory);
		/*
		 *
		for(int i=0; i<xfer_len; i++) {
			printLog("%02X ", *(data+4+i));
		}
		printLog("\r\n");
		*/
	}
}
void send_data()
{
	uint32_t len = encounter_count<<5;
	uint8 data[256];
	uint16 result;
    int xfer_len = _min_packet_size & 0xFC;  // Make sure it is divisible by 4
    uint32_t start = ts_ms();
	uint32_t addr=0;
	int bad=0;
    int max_data_len = xfer_len - 4;
    int packet_index = 0;

	printLog("packet xfer_len: %d\r\n", xfer_len);
	printLog("len: %ld\r\n", len);
    while (len>0) {
		if (len <= max_data_len) xfer_len = len+4;
		memcpy(data, &packet_index, 4);
		printLog("sent packet_idx %d\r\n", packet_index);
		packet_index++;
		int32_t retCode = storage_readRaw(addr, data+4, xfer_len-4);
		if (retCode)  bad++;
		do {
			result = gecko_cmd_gatt_server_send_characteristic_notification(_conn_handle, gattdb_gatt_spp_data, xfer_len, data)->result;
			_sCounters.num_writes++;
			// printLog("send result: %u\r\n", result);
		} while(result == bg_err_out_of_memory);

		if (result != 0) {
			printLog("Unexpected error: %x\r\n", result);
		} else {
			_sCounters.num_pack_sent++;
			_sCounters.num_bytes_sent += (xfer_len-4);
		}
        addr += (xfer_len-4);
        len -= (xfer_len-4);
    	printLog("addr: %ld, len: %ld\r\n", addr, len);
    	if (packet_index%8 ==0) {
    		// wait(500);
    	}
	}
    printLog("Done xfer time:%ld\r\n",  ts_ms()-start);
	printLog("number of bad memory reads in xfer: %d\r\n", bad);

}

void send_data_turbo(uint32_t start_idx, uint32_t number_of_packets)
{
	uint32_t len = encounter_count<<5;
	uint8 data[256];
	uint16 result;
    int xfer_len = _min_packet_size & 0xFC;  // Make sure it is divisible by 4
    uint32_t start_ts = ts_ms();
	uint32_t addr=0;
	int bad=0;
    int max_data_len = xfer_len - 4;
    int packet_index = 0;

    // calculate max index
    int max_index = len / (xfer_len - 4);
	if (len % (xfer_len - 4) == 0)
		max_index--;
    if (start_idx > max_index) {
    	// don't do anything???
    	return;
	} else {
		// calculate number of bytes to read from memory
		len = number_of_packets * (xfer_len - 4);
		addr = start_idx * (xfer_len - 4);
		// check if len is greater than what is left ot transfer
		if ( (encounter_count<<5) - addr < len) {
			len = (encounter_count<<5) - addr;
		}
		while (len > 0) {
			if (len <= max_data_len)
				xfer_len = len + 4;
			memcpy(data, &packet_index, 4);
			printLog("sent packet_idx %d\r\n", packet_index);
			packet_index++;
			int32_t retCode = storage_readRaw(addr, data + 4, xfer_len - 4);
			if (retCode)
				bad++;
			do {
				result =
						gecko_cmd_gatt_server_send_characteristic_notification(
								_conn_handle, gattdb_gatt_spp_data, xfer_len,
								data)->result;
				_sCounters.num_writes++;
				// printLog("send result: %u\r\n", result);
			} while (result == bg_err_out_of_memory);

			if (result != 0) {
				printLog("Unexpected error: %x\r\n", result);
			} else {
				_sCounters.num_pack_sent++;
				_sCounters.num_bytes_sent += (xfer_len - 4);
			}
			addr += (xfer_len - 4);
			len -= (xfer_len - 4);
			printLog("addr: %ld, len: %ld\r\n", addr, len);
		}
		printLog("Done xfer time:%ld\r\n", ts_ms() - start_ts);
		printLog("number of bad memory reads in xfer: %d\r\n", bad);
	}
}

uint32_t em(uint32_t t) {
	return ((t-offset_time) / 1000 + epochtimesync)/60;
}

void read_name_ps(void) {
	uint16 k = 0x4000;
	uint8_t *name;
	struct gecko_msg_flash_ps_load_rsp_t *result;
    result = gecko_cmd_flash_ps_load(k);
    printLog("read_name_ps 0x%04X\r\n", result->result);
    if (result->result==0) {
    	if (result->value.len==8) {
    		name = result->value.data;
    		gecko_cmd_gatt_server_write_attribute_value(gattdb_device_name, 0, 8, name);
    	} else { printLog("Problem readinig BT name\r\n"); }
    }
}

void write_name_ps(uint8_t *name) {
	uint16_t key = 0x4000;
	int result = gecko_cmd_flash_ps_save(key, 8, name)->result;
    printLog("write_name_ps 0x%04X\r\n", result);

}

void set_name(uint8_t *name) {
	// uint8_t buffer[255];
	/*
	struct gecko_msg_gatt_server_read_attribute_value_rsp_t *result;
	result = gecko_cmd_gatt_server_read_attribute_value(gattdb_device_name, 0);
	printLog("%d, %d\r\n",  result->result, result->value.len);
	memset(buffer, 0, 255);
	memcpy(buffer, result->value.data, result->value.len);
	printLog("name: %s\r\n", buffer);
	memset(buffer, '0', 8);
	printLog("test zero pad:%08d\r\n", 0);
	*/
	write_name_ps(name);
	gecko_cmd_gatt_server_write_attribute_value(gattdb_device_name, 0, 8, name);
	/*
	result = gecko_cmd_gatt_server_read_attribute_value(gattdb_device_name, 0);
	printLog("result: %d, %d\r\n",  result->result, result->value.len);
	memset(buffer, 0, 255);
	memcpy(buffer, result->value.data, result->value.len);
	printLog("name: %s\r\n", buffer);
	*/
}
void start_writing_flash(uint8_t *data, uint8_t len) {
	write_flash = true;
	p_fifo_last_idx = c_fifo_last_idx;
	printLog("Start writing to flash\r\n");
	uint8_t blank[32];
	memset(blank, 0, 32);
	store_event(blank);
	if (mode==MODE_ENCOUNTER) {
		store_event(blank);
	}
	store_time(data, len);
}

const char *version_str = "Version: " __DATE__ " " __TIME__;
const char *ota_version = "1.4.2";
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
	case 'f':{
		printLog("mode: send data ota\r\n");
		// SLEEP_SleepBlockBegin(sleepEM2); // Disable sleeping
		// send_data();  // over bluetooth
		// SLEEP_SleepBlockEnd(sleepEM2); // Enable sleeping
        sending_ota = true;

        break;
	}
	case 'F':{
		printLog("mode: Set data ota false\r\n");
        sending_ota = false;
        break;
	}
	case 't':{
		printLog("mode: send data turbo\r\n");
		// SLEEP_SleepBlockBegin(sleepEM2); // Disable sleeping
		// sending_turbo = true;
		send_data();  // over bluetooth
		// SLEEP_SleepBlockEnd(sleepEM2); // Enable sleeping
        break;
	}
	case 'T':{
		printLog("mode: place holder for end send turbo\r\n");
		// sending_turbo = false;
        break;
	}
	case 'u':{
		printLog("send mtu\r\n");
		send_ota_uint8(_min_packet_size);
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
	case 'E':{
		mode = MODE_ENCOUNTER;
		write_flash = false;
		printLog("Set to mode to ENCOUNTER\r\n");
        break;
	}
	case 'R':{
		mode = MODE_RAW;
		write_flash = false;
		printLog("Set to mode to RAW\r\n");
        break;
	}
	case 'm':{
		uint8_t value=mode;
		send_ota_uint8(value);
		printLog("sent mode value: %d\r\n", value);
		break;
	}
	case 'I':{
		uint8_t value=0;

		if (write_flash) {
			value = 1;
			if (mark_set) {
				value = 3;
			}
		}
		send_ota_uint8(value);
		printLog("sent write_flash value: %d\r\n", value);
		break;
	}
	case 'w':{
		if (!write_flash) {
			uint8_t *data = gecko_cmd_gatt_server_read_attribute_value(gattdb_gatt_spp_data,0 )->value.data;
			uint8_t len = gecko_cmd_gatt_server_read_attribute_value(gattdb_gatt_spp_data,0 )->value.len;

			start_writing_flash(data, len);
		}
        break;
	}
	case 's':{
		write_flash = false;
		mark_set = false;
		printLog("Stop writing to flash\r\n");
        break;
	}
	case 'a': {
		uint32_t ts = ts_ms();
		send_uint32(ts);
		send_uint32(time_overflow);

		break;
	}
	case 'A': {
		uint32_t ts = ts_ms();
		send_ota_uint32(ts);
		send_ota_uint32(time_overflow);
		printLog("ts, overflow: %ld, %ld\r\n", ts, time_overflow);
		break;
	}
	case 'C': {
		flash_erase();
		// if (encounter_count==0) printLog("Flash is empty\r\n");
		break;
	}
	case 'o': {
		uint8_t time_evt[32];
        get_uint32(&epochtimesync);
        get_uint32(&offset_time);
        get_uint32(&offset_overflow);
		printLog("epochtime offset_times: %ld %ld %ld\r\n", epochtimesync, offset_time, offset_overflow);
		memset(time_evt, 0, 32);
		memcpy(time_evt+4, &epochtimesync, 4);
		memcpy(time_evt+8, &offset_time, 4);
		memcpy(time_evt+12, &offset_overflow, 4);
	    store_event(time_evt);
	    setup_next_minute();
		break;
	}
	case 'B': {
		uint16_t *scan_params;

		scan_params = (uint16_t *) gecko_cmd_gatt_server_read_attribute_value(gattdb_gatt_spp_data,0 )->value.data;
		printLog("Change bluetooth scanning parameters interval, window: %u, %u\r\n", scan_params[0], scan_params[1]);
		startObserving(scan_params[0], scan_params[1]);
		break;
	}
	case 'P': {
		uint16_t *conn_params;

		conn_params = (uint16_t *) gecko_cmd_gatt_server_read_attribute_value(gattdb_gatt_spp_data,0 )->value.data;
		printLog("Change bluetooth connection parameters min %u, max %u, timeout min %u\r\n", conn_params[0], conn_params[1], conn_params[2]);
		setConnectionTiming(conn_params);
		break;
	}
	case 'O': {
		// uint8_t time_evt[32];
		uint32_t *timedata;
		uint32_t ts = ts_ms();
		timedata = (uint32_t *) gecko_cmd_gatt_server_read_attribute_value(gattdb_gatt_spp_data,0 )->value.data;
		uint8_t len = gecko_cmd_gatt_server_read_attribute_value(gattdb_gatt_spp_data,0 )->value.len;
        printLog("In O, got %d bytes, ts: %ld\r\n", len, ts);
        epochtimesync = timedata[0]; // units are sec
        offset_time = timedata[1];  // units are ms
        offset_overflow = timedata[2];  // units are integers

        uint32_t epoch_minute_origin = (epochtimesync)/60;
        uint32_t extra_seconds = epochtimesync%60;
        next_minute = offset_time - extra_seconds * 1000 + 60000;
        uint32_t dt = (ts-offset_time)/1000;
        uint32_t epoch_minute = (dt + epochtimesync)/60;
        printLog("em(ts), em(next_minute) %ld, %ld\r\n", em(ts), em(next_minute+1000));

        printLog("offset_time, timestamp, next minute: %ld, %ld, %ld %ld\r\n", offset_time, ts, next_minute, extra_seconds);

		printLog("epochtimes, e_min, e_min_origin: %ld %ld %ld\r\n", epochtimesync, epoch_minute, epoch_minute_origin);
        // store_time();
		break;
	}

	case 'N': {
		uint8_t *new_name;
		new_name = (uint8_t *) gecko_cmd_gatt_server_read_attribute_value(gattdb_gatt_spp_data,0 )->value.data;
		set_name(new_name);
		printLog("set new name\r\n");
		break;
	}
	case 'M': {
        gecko_external_signal(LOG_MARK);
        mark_set = true;
        break;
	}
	case 'U': {
        gecko_external_signal(LOG_UNMARK);
        mark_set = false;
        break;
	}
    case 'h':
        printLog("%s\r\n", version_str);
        break;
    case 'v': {
        send_ota_msg((char *)ota_version);
        break;
    }
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
		// printLog("t: %lu rssi: %d channel %d\r\n", timestamp, rssi, channel);
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
//#define SCAN_DEBUG
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

    //if (true) {

	if(findServiceInAdvertisement(evt->data.evt_le_gap_extended_scan_response.data.data,
									evt->data.evt_le_gap_extended_scan_response.data.len)) {

#ifdef SCAN_DEBUG
	bool found_evt = findServiceInAdvertisement(evt->data.evt_le_gap_extended_scan_response.data.data,
									evt->data.evt_le_gap_extended_scan_response.data.len);
    printk("\tfinish parse data %d, len: %d\r\n", found_evt, evt->data.evt_le_gap_extended_scan_response.data.len);
#endif
        Encounter_record *current_encounter;
        if (timestamp>next_minute) update_next_minute();
        int sec_timestamp = (timestamp - (next_minute - 60000)) / 1000;
        uint32_t epoch_minute = ((timestamp-offset_time) / 1000 + epochtimesync)/60;

        // printLog("epoch minute, sec: %ld, %d\r\n", epoch_minute, sec_timestamp);

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
    		// printLog("first saewoo_hack: %d\r\n", saewoo_hack);
            current_encounter = encounters + (c_fifo_last_idx & IDX_MASK);
            memset((uint8_t *)current_encounter, 0, 64);  // clear all values
            memcpy(current_encounter->mac, mac_addr, 6);
            current_encounter->rssi_data[saewoo_hack].n = 1;
            current_encounter->rssi_data[saewoo_hack].max = rssi;
            current_encounter->rssi_data[saewoo_hack].min = rssi;
            current_encounter->rssi_data[saewoo_hack].mean = rssi;
            current_encounter->rssi_data[saewoo_hack].var = 0;
            current_encounter->first_time = sec_timestamp;
            current_encounter->last_time = 0xFF;
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
            if (old_n == 0) {  // there could be a rollover problem here...  it will erase data
            	// printLog("saewoo_hack: %d\r\n", saewoo_hack);
                current_encounter->rssi_data[saewoo_hack].max = rssi;
                current_encounter->rssi_data[saewoo_hack].min = rssi;
                current_encounter->rssi_data[saewoo_hack].mean = rssi;
                current_encounter->rssi_data[saewoo_hack].var = 0;
            } else {
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
        }
        // printLog("text:rssi, ch: %d, %d\r\n", rssi, channel);
#ifdef SCAN_DEBUG
        uart_ch_rssi(current_encounter->rssi_data[saewoo_hack]);
#endif
        // Update bobs shared key
#ifdef SCAN_DEBUG
    printk("\tDo crypto stuff flag: %02X\n", current_encounter->flag);
#endif
        uint8_t hi_lo_byte = evt->data.evt_le_gap_extended_scan_response.data.data[30];
#ifdef SCAN_DEBUG
    for(int index=26; index<31; index++) {
    	printk("\t%d: %02X\r\n", index, evt->data.evt_le_gap_extended_scan_response.data.data[index]);
    }
    printk("\thi_lo_ byte %d\n", hi_lo_byte);
#endif
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
        // memset(current_encounter->public_key, 0xFF, 32);
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
    uint32_t epoch_minute = ((timestamp-offset_time) / 1000 + epochtimesync)/60;
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
        	/*
        	send32bytes(ptr);
        	send32bytes(ptr+32);
        	*/
        	store_event(ptr);
        	store_event(ptr+32);

        	p_fifo_last_idx++;
        } else {
            // uart_printf("Done flash_store looking back\n");
            return;
        }
    }
}

static void button_callback(const uint8_t pin)
{
  if (pin == BSP_BUTTON0_PIN) {
    /* when input is high, the button was released */
    if (GPIO_PinInGet(BSP_BUTTON0_PORT,BSP_BUTTON0_PIN)) {
        // gecko_external_signal(BUTTON_RELEASED);
        gecko_external_signal(LOG_RESET);

    }
    /* when input is low, the button was pressed*/
    else {
        gecko_external_signal(BUTTON_PRESSED);
    }
  }
  if (pin == BSP_BUTTON1_PIN) {
    /* when input is high, the button was released */
    if (GPIO_PinInGet(BSP_BUTTON0_PORT,BSP_BUTTON0_PIN)) {
        gecko_external_signal(LOG_MARK);
    }
    /* when input is low, the button was pressed*/
    else {
    	;
    }
  }
  if (pin == BSP_BUTTON2_PIN) {
    /* when input is high, the button was released */
    if (GPIO_PinInGet(BSP_BUTTON0_PORT,BSP_BUTTON0_PIN)) {
        gecko_external_signal(LOG_UNMARK);
    }
    /* when input is low, the button was pressed*/
    else {
    	;
    }
  }

}

typedef struct ButtonArray{
  GPIO_Port_TypeDef   port;
  unsigned int        pin;
} ButtonArray_t;

static const ButtonArray_t buttonArray[BSP_BUTTON_COUNT] = BSP_BUTTON_INIT;

// void gpioCallback(uint8_t pin);

void init_GPIO_buttons() {
	GPIOINT_Init();
	for (int i = 0; i < BSP_BUTTON_COUNT; i++) {
		GPIO_PinModeSet(buttonArray[i].port, buttonArray[i].pin,
				gpioModeInputPullFilter, 1);
		GPIO_ExtIntConfig(buttonArray[i].port, buttonArray[i].pin,
				buttonArray[i].pin, true, true, true);
		GPIOINT_CallbackRegister(buttonArray[i].pin, button_callback);

	}
	/*
	GPIOINT_CallbackRegister(buttonArray[0].pin, gpioCallback);
	GPIOINT_CallbackRegister(buttonArray[1].pin, gpioCallback);
	GPIO_IntConfig(buttonArray[0].port, buttonArray[0].pin, false, true, true);
	GPIO_IntConfig(buttonArray[1].port, buttonArray[1].pin, false, true, true);
	*/
}

void init_GPIO(void) {
	/* Initialize GPIO interrupt handler */
	GPIOINT_Init();

	/* Set the pin of Push Button 0 as input with pull-up resistor */
	GPIO_PinModeSet( BSP_BUTTON0_PORT, BSP_BUTTON0_PIN, HAL_GPIO_MODE_INPUT_PULL_FILTER, HAL_GPIO_DOUT_HIGH );

	/* Enable interrupt on Push Button 0 */
	GPIO_ExtIntConfig(BSP_BUTTON0_PORT, BSP_BUTTON0_PIN, BSP_BUTTON0_PIN, true, true, true);

	/* Register callback for Push Button 0 */
	GPIOINT_CallbackRegister(BSP_BUTTON0_PIN, button_callback);

}
/**
 * @brief setup_leds
 * Configure LED pins as output
 */
static void init_leds(void) {
  GPIO_PinModeSet(BSP_LED0_PORT, BSP_LED0_PIN, gpioModePushPull, 0);
  // GPIO_PinModeSet(BSP_LED1_PORT, BSP_LED1_PIN, gpioModePushPull, 0);
}

/**
 * @brief get_leds
 * Get LED statuses as two least significant bits of a return byte.
 * @return uint8_t LED status byte.
 */
static inline uint8_t get_leds(void) {
    return GPIO_PinOutGet(BSP_LED0_PORT, BSP_LED0_PIN);
    // return ((GPIO_PinOutGet(BSP_LED1_PORT, BSP_LED1_PIN) << 1) | GPIO_PinOutGet(BSP_LED0_PORT, BSP_LED0_PIN));
}

/**
 * @brief LED control function
 * bit 0 = LED0
 * bit 1 = LED1
 * bits 2-7 = don't care
 */
static void set_leds(uint8_t control_byte) {

  /* LED 0 control */
  if ((control_byte & 0x01) == 1) {
    GPIO_PinOutSet(BSP_LED0_PORT, BSP_LED0_PIN);
  } else {
    GPIO_PinOutClear(BSP_LED0_PORT, BSP_LED0_PIN);
  }

  /* LED 1 control
  if (((control_byte >> 1) & 0x01) == 1) {
    GPIO_PinOutSet(BSP_LED1_PORT, BSP_LED1_PIN);
  } else {
    GPIO_PinOutClear(BSP_LED1_PORT, BSP_LED1_PIN);
  }
  */
}

void led_flash(int n) {
	flashes = n;
	if (n==0) {
		gecko_cmd_hardware_set_soft_timer(0,HANDLE_LED,0);
	} else {
		if (get_leds()) {
		  // led is on, so turn it off
		  set_leds(0);
		  printLog("LED OFF\r\n");
		  flashes--;
		  if (flashes) gecko_cmd_hardware_set_soft_timer(6000, HANDLE_LED, 1);
		  else gecko_cmd_hardware_set_soft_timer(0,HANDLE_LED,0);
		} else {
		    printLog("LED ON\r\n");
		    set_leds(1);
		    gecko_cmd_hardware_set_soft_timer(3000, HANDLE_LED, 1);
		}
	}
}

/* Main application */
void appMain(gecko_configuration_t *pconfig)
{
#if DISABLE_SLEEP > 0
  pconfig->sleep.flags = 0;
#endif

  /* Initialize debug prints. Note: debug prints are off by default. See DEBUG_LEVEL in app.h */
  initLog();
#ifdef THREE_BUTTONS
  init_GPIO_buttons();
#else
  init_GPIO();
#endif
  init_leds();
  adcInit();
  readBatteryLevel();
  printLog("BatteryLevel %d\r\n", battBatteryLevel);
  /* Initialize stack */
  gecko_init(pconfig);
  int32_t flash_ret = storage_init();
  uint32_t flash_size = storage_size();
  printLog("storage_init: %ld %ld\r\n", flash_ret, flash_size);
  determine_counts(flash_size);
  test_encrypt_compare();
  read_name_ps();
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
	    	// prep data xfer
			reset_variables();
	    	gecko_cmd_gatt_set_max_mtu(247);
	    	// Not sure how to advertise
	    	// gecko_cmd_le_gap_start_advertising(HANDLE_DEMO, le_gap_general_discoverable, le_gap_undirected_connectable);

			/* Start general advertising and enable connections. */
			gecko_cmd_le_gap_start_advertising(HANDLE_DEMO, le_gap_general_discoverable, le_gap_connectable_scannable);
			/*  Start advertising non-connectable packets */
			bcnSetupAdvBeaconing();
			// 2.4 seconds and 200ms
			startObserving(320*12, 320);
			break;

	      case gecko_evt_system_external_signal_id:
			if (evt->data.evt_system_external_signal.extsignals & LOG_MARK) {
				printLog("MARK\r\n");
			    Encounter_record *current_encounter;
				uint32_t timestamp = ts_ms();
		        if (timestamp>next_minute) update_next_minute();
		        int sec_timestamp = (timestamp - (next_minute - 60000)) / 1000;
		        uint32_t epoch_minute = ((timestamp-offset_time) / 1000 + epochtimesync)/60;

                uint8_t mac_addr[6] = {0, 0, 0, 0, 0, 0};
	            current_encounter = encounters + (c_fifo_last_idx & IDX_MASK);
	            memset((uint8_t *)current_encounter, 0, 64);  // clear all values
	            memcpy(current_encounter->mac, mac_addr, 6);
	            current_encounter->first_time = sec_timestamp;
	            current_encounter->last_time = sec_timestamp;
	            current_encounter->minute = epoch_minute;
	            current_encounter->flag = 0x07;
	            memset(current_encounter->public_key, 1, 32);
	            c_fifo_last_idx++;
			}
			if (evt->data.evt_system_external_signal.extsignals & LOG_UNMARK) {
				printLog("UNMARK\r\n");
			    Encounter_record *current_encounter;
				uint32_t timestamp = ts_ms();
		        if (timestamp>next_minute) update_next_minute();
		        int sec_timestamp = (timestamp - (next_minute - 60000)) / 1000;
		        uint32_t epoch_minute = ((timestamp-offset_time) / 1000 + epochtimesync)/60;

                uint8_t mac_addr[6] = {1,1,1,1,1,1};
	            current_encounter = encounters + (c_fifo_last_idx & IDX_MASK);
	            memset((uint8_t *)current_encounter, 0, 64);  // clear all values
	            memcpy(current_encounter->mac, mac_addr, 6);
	            current_encounter->first_time = sec_timestamp;
	            current_encounter->last_time = sec_timestamp;
	            current_encounter->minute = epoch_minute;
	            current_encounter->flag = 0x07;
	            memset(current_encounter->public_key, 2, 32);
	            c_fifo_last_idx++;
			}

			if (evt->data.evt_system_external_signal.extsignals & LOG_RESET) {
				printLog("RESET MARK\r\n");
			    Encounter_record *current_encounter;
				uint32_t timestamp = ts_ms();
		        if (timestamp>next_minute) update_next_minute();
		        int sec_timestamp = (timestamp - (next_minute - 60000)) / 1000;
		        uint32_t epoch_minute = ((timestamp-offset_time) / 1000 + epochtimesync)/60;

                uint8_t mac_addr[6] = {2, 2, 2, 2, 2, 2};
	            current_encounter = encounters + (c_fifo_last_idx & IDX_MASK);
	            memset((uint8_t *)current_encounter, 0, 64);  // clear all values
	            memcpy(current_encounter->mac, mac_addr, 6);
	            current_encounter->first_time = sec_timestamp;
	            current_encounter->last_time = sec_timestamp;
	            current_encounter->minute = epoch_minute;
	            current_encounter->flag = 0x07;
	            memset(current_encounter->public_key, 3, 32);
	            c_fifo_last_idx++;
			}


	        if (evt->data.evt_system_external_signal.extsignals & BUTTON_PRESSED) {
	        	printLog("Button Pressed\r\n");
	        }

	        if (evt->data.evt_system_external_signal.extsignals & BUTTON_RELEASED) {
	        	printLog("Button released (reset)\r\n");
	        	if (false) { // disable start recording with button press
	        		clicks += 1;
	        		gecko_cmd_hardware_set_soft_timer(32768>>2,HANDLE_CLICK,0);
	        	}
	        }

	        break;

		  case gecko_evt_le_gap_extended_scan_response_id: {
			  // printLog("scan event\r\n");
			  if (mode==MODE_ENCOUNTER) {
				  process_scan_encounter(evt);
			  } else {
				  process_scan(evt);
			  }
			  break;
		  }
		  case gecko_evt_hardware_soft_timer_id:
		  {
			  uint32_t ts = ts_ms();
			  if (mode==MODE_ENCOUNTER) {
				  if (ts>next_minute) update_next_minute();
				  readBatteryLevel();
			  }

			  switch(evt->data.evt_hardware_soft_timer.handle) {
			  case HANDLE_ADV:
				  newRandAddress();
				  if (write_flash) flash_store();
				  break;
			  case HANDLE_UPDATE_KEY:
				  update_adv_key();
				  break;
			  case HANDLE_LED:
				  // toggle state of led
				  printLog("Handle_LED: %d\r\n", flashes);
				  led_flash(flashes);
				  break;
			  case HANDLE_CLICK:
				  // handle number of clicks
				  switch (clicks) {
				  case 1:
					  printLog("Single click\r\n");
					  led_flash(1);
					  // start_writing_flash();
					  //write_flash = true;
					  // gecko_cmd_hardware_set_soft_timer(0,HANDLE_CLICK,0);
					  break;
				  case 2:
					  printLog("Double click\r\n");
					  led_flash(2);
					  write_flash = false;
					  //gecko_cmd_hardware_set_soft_timer(0,HANDLE_CLICK,0);
					  break;
				  case 0:
					  gecko_cmd_hardware_set_soft_timer(0,HANDLE_CLICK,0);
					  break;
				  default:
					  printLog("clicks %d\r\n", clicks);
				  }
				  // reset click counter

				  clicks = 0;
				  break;
			  case HANDLE_UART:  // not used
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

		    	_conn_handle = evt->data.evt_le_connection_opened.connection;
		    	printLog("Connected\r\n");
		    	_main_state = STATE_CONNECTED;

		    	/* Request connection parameter update.
		    	 * conn.interval min 20ms, max 40ms, slave latency 4 intervals,
		    	 * supervision timeout 2 seconds
		    	 * (These should be compliant with Apple Bluetooth Accessory Design Guidelines, both R7 and R8) */
		    	// original
		    	// gecko_cmd_le_connection_set_timing_parameters(_conn_handle, 24, 40, 0, 200, 0, 0xFFFF);
		    	// actually 20ms and 40 ms
		    	//gecko_cmd_le_connection_set_timing_parameters(_conn_handle, 16, 32, 0, 200, 0, 0xFFFF);
		    	// 15ms, 30ms, 100ms
		    	// gecko_cmd_le_connection_set_timing_parameters(_conn_handle, 12, 24, 0, 10, 0, 0xFFFF);
		    	// 15ms, 30ms, 100ms
		    	gecko_cmd_le_connection_set_timing_parameters(_conn_handle, 6, 6, 0, 10, 0, 0xFFFF);

			break;

		  case gecko_evt_le_connection_parameters_id:
		    	printLog("Conn.parameters: interval %u units, %u latency, txsize %u\r\n",
		    			evt->data.evt_le_connection_parameters.interval,
		    			evt->data.evt_le_connection_parameters.latency,
						evt->data.evt_le_connection_parameters.txsize);
		    	break;

		  case gecko_evt_gatt_mtu_exchanged_id:
				/* Calculate maximum data per one notification / write-without-response, this depends on the MTU.
				 * up to ATT_MTU-3 bytes can be sent at once  */
				_max_packet_size = evt->data.evt_gatt_mtu_exchanged.mtu - 3;
				_min_packet_size = _max_packet_size & 0xFC; // make 32bit wide /* Try to send maximum length packets whenever possible */
				printLog("MTU exchanged: %d\r\n", evt->data.evt_gatt_mtu_exchanged.mtu);
				break;

		  case gecko_evt_le_connection_closed_id:

			printLog("connection closed, reason: 0x%2.2x\r\n", evt->data.evt_le_connection_closed.reason);

			/* Check if need to boot to OTA DFU mode */
			if (boot_to_dfu) {
			  /* Enter to OTA DFU mode */
			  gecko_cmd_system_reset(2);
			} else {
				// File transfer stuff
		    	// printStats(&_sCounters);
		    	reset_variables();
		    	// SLEEP_SleepBlockEnd(sleepEM2); // Enable sleeping

		    	/* Restart advertising after client has disconnected */
		    	// gecko_cmd_le_gap_start_advertising(HANDLE_DEMO, le_gap_general_discoverable, le_gap_undirected_connectable);


			  gecko_cmd_le_gap_start_advertising(HANDLE_DEMO, le_gap_general_discoverable, le_gap_connectable_scannable);
			}
			break;

		  case gecko_evt_gatt_server_characteristic_status_id:
		    {
		    	struct gecko_msg_gatt_server_characteristic_status_evt_t *pStatus;
		    	pStatus = &(evt->data.evt_gatt_server_characteristic_status);

		    	printLog("gecko_evt_gatt_server_characteristic_status_id %d\r\n", pStatus->characteristic);

		    	if (pStatus->characteristic == gattdb_gatt_spp_data) {
		    		if (pStatus->status_flags == gatt_server_client_config) {
		    			printLog("client_config_flags %d\r\n", pStatus->client_config_flags);
		    			// Characteristic client configuration (CCC) for spp_data has been changed
		    			if (pStatus->client_config_flags == gatt_notification) {
		    				_main_state = STATE_SPP_MODE;
		    				// SLEEP_SleepBlockBegin(sleepEM2); // Disable sleeping
		    				printLog("SPP Mode ON\r\n");
		    			} else {
		    				printLog("SPP Mode OFF\r\n");
		    				_main_state = STATE_CONNECTED;
		    				// SLEEP_SleepBlockEnd(sleepEM2); // Enable sleeping
		    			}

		    		}
		    	}
		    }
		    break;

		  case gecko_evt_gatt_server_attribute_value_id:
		  {
			  if (evt->data.evt_gatt_server_attribute_value.attribute==gattdb_Read_Write) {
				  int c = evt->data.evt_gatt_server_attribute_value.value.data[0];
				  printLog("new rw value %c\r\n", c);
				  parse_command(c);
			  } else if (evt->data.evt_gatt_server_attribute_value.attribute==gattdb_gatt_spp_data) {
				  if (sending_ota) {
					  uint32_t *index;
					  int len = evt->data.evt_gatt_server_attribute_value.value.len;
					  if (len==4) {
						  index = (uint32_t *) evt->data.evt_gatt_server_attribute_value.value.data;
						  // printLog("Got request for data, len %d, idx: %ld\r\n", evt->data.evt_gatt_server_attribute_value.value.len, *index);
						  send_chunk(*index);
					  } else { printLog("Recevied the wrong number of bytes: %d/4\r\n", len); }
				  } else if (sending_turbo) {
					  uint32_t *params;
					  int len = evt->data.evt_gatt_server_attribute_value.value.len;
					  if (len==8) {
						  params = (uint32_t *) evt->data.evt_gatt_server_attribute_value.value.data;
						  printLog("Got request for turbo data, len %d, idx: %lu len: %lu\r\n",
								  evt->data.evt_gatt_server_attribute_value.value.len, params[0], params[1]);
						  send_data_turbo(params[0], params[1]);
					  } else { printLog("Recevied the wrong number of bytes: %d/8\r\n", len); }
				  } else {
					  char *msg;
					  msg = (char *) evt->data.evt_gatt_server_attribute_value.value.data;
					  printLog("new message: %s\r\n", msg);
				  }
			  } else if (evt->data.evt_gatt_server_attribute_value.attribute==gattdb_data_in) {


				   if (sending_ota) {
					  uint32_t *index;
					  int len = evt->data.evt_gatt_server_attribute_value.value.len;
					  if (len==4) {
						  index = (uint32_t *) evt->data.evt_gatt_server_attribute_value.value.data;
						  // printLog("Got request for packet, len %d, idx: %ld\r\n", evt->data.evt_gatt_server_attribute_value.value.len, *index);
						  send_chunk(*index);
					  } else { printLog("Recevied the wrong number of bytes: %d/4\r\n", len); }
				  } else {
					  char *msg;
					  msg = (char *) evt->data.evt_gatt_server_attribute_value.value.data;
					  printLog("gatt_data_in new message: %s\r\n", msg);
				  }

				  printLog("gatt data_in\r\n");
			  }
			  // printLog("Done attribute_value_id\r\n");
			  // parse_command(c);

			  break;
		  }

	      case gecko_evt_gatt_server_user_read_request_id:
		    printLog("user read request characteristic: %d\r\n",
		    		(evt->data.evt_gatt_server_user_read_request.characteristic) );
	        if(evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_battery_level) {
	          gecko_cmd_gatt_server_send_user_read_response(evt->data.evt_gatt_server_user_read_request.connection,
	              evt->data.evt_gatt_server_user_read_request.characteristic, 0, 1, &battBatteryLevel);
	        }
	        break;

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

// from https://docs.silabs.com/resources/bluetooth/code-examples/applications/reporting-battery-voltage-over-ble/source/app.c
#if defined(ADC_PRESENT)
/* 5V reference voltage, no attenuation on AVDD, 12 bit ADC data */
  #define ADC_SCALE_FACTOR   (5.0 / 4095.0)
#elif defined(IADC_PRESENT)
/* 1.21V reference voltage, AVDD attenuated by a factor of 4, 12 bit ADC data */
  #define ADC_SCALE_FACTOR   (4.84 / 4095.0)
#endif

/**
 * @brief Initialise ADC. Called after boot in this example.
 */

static void adcInit(void)
{
#if defined(ADC_PRESENT)
  ADC_Init_TypeDef init = ADC_INIT_DEFAULT;
  ADC_InitSingle_TypeDef initSingle = ADC_INITSINGLE_DEFAULT;

  CMU_ClockEnable(cmuClock_ADC0, true);
  CMU_ClockEnable(cmuClock_PRS, true);

  /* Only configure the ADC if it is not already running */
  if ( ADC0->CTRL == _ADC_CTRL_RESETVALUE ) {
    init.timebase = ADC_TimebaseCalc(0);
    init.prescale = ADC_PrescaleCalc(1000000, 0);
    ADC_Init(ADC0, &init);
  }

  initSingle.acqTime = adcAcqTime16;
  initSingle.reference = adcRef5VDIFF;
  initSingle.posSel = adcPosSelAVDD;
  initSingle.negSel = adcNegSelVSS;
  initSingle.prsEnable = true;
  initSingle.prsSel = adcPRSSELCh4;

  ADC_InitSingle(ADC0, &initSingle);
#elif defined(IADC_PRESENT)
  IADC_Init_t init = IADC_INIT_DEFAULT;
  IADC_AllConfigs_t allConfigs = IADC_ALLCONFIGS_DEFAULT;
  IADC_InitSingle_t initSingle = IADC_INITSINGLE_DEFAULT;
  IADC_SingleInput_t input = IADC_SINGLEINPUT_DEFAULT;

  CMU_ClockEnable(cmuClock_IADC0, true);
  CMU_ClockEnable(cmuClock_PRS, true);

  /* Only configure the ADC if it is not already running */
  if ( IADC0->CTRL == _IADC_CTRL_RESETVALUE ) {
    IADC_init(IADC0, &init, &allConfigs);
  }

  input.posInput = iadcPosInputAvdd;

  IADC_initSingle(IADC0, &initSingle, &input);
  IADC_enableInt(IADC0, IADC_IEN_SINGLEDONE);
#endif
  return;
}


/***************************************************************************//**
 * @brief
 *    Initiates an A/D conversion and reads the sample when done
 *
 * @return
 *    The output of the A/D conversion
 ******************************************************************************/
static uint16_t getAdcSample(void)
{
#if defined(ADC_PRESENT)
  ADC_Start(ADC0, adcStartSingle);
  while ( !(ADC_IntGet(ADC0) & ADC_IF_SINGLE) )
    ;

  return ADC_DataSingleGet(ADC0);
#elif defined(IADC_PRESENT)
  /* Clear single done interrupt */
  IADC_clearInt(IADC0, IADC_IF_SINGLEDONE);

  /* Start conversion and wait for result */
  IADC_command(IADC0, IADC_CMD_SINGLESTART);
  while ( !(IADC_getInt(IADC0) & IADC_IF_SINGLEDONE) ) ;

  return IADC_readSingleData(IADC0);
#endif
}


/***************************************************************************//**
 * @brief
 *    Measures the supply voltage by averaging multiple readings
 *
 * @param[in] avg
 *    Number of measurements to average
 *
 * @return
 *    The measured voltage
 ******************************************************************************/
float SUPPLY_measureVoltage(unsigned int avg)
{
  uint16_t adcData;
  float supplyVoltage;
  int i;

  adcInit();

  supplyVoltage = 0;

  for ( i = 0; i < avg; i++ ) {
    adcData = getAdcSample();
    supplyVoltage += (float) adcData * ADC_SCALE_FACTOR;
  }

  supplyVoltage = supplyVoltage / (float) avg;

  return supplyVoltage;
}

typedef struct {
  float         voltage;
  uint8_t       capacity;
} VoltageCapacityPair;

static VoltageCapacityPair battCR2032Model[] =
{ { 3.0, 100 }, { 2.9, 80 }, { 2.8, 60 }, { 2.7, 40 }, { 2.6, 30 },
  { 2.5, 20 }, { 2.4, 10 }, { 2.0, 0 } };

static uint8_t battBatteryLevel; /* Battery Level */

/***************************************************************************************************
 * Static Function Declarations
 **************************************************************************************************/
// static uint8_t readBatteryLevel(void);

static uint8_t calculateLevel(float voltage, VoltageCapacityPair *model, uint8_t modelEntryCount)
{
  uint8_t res = 0;

  if (voltage >= model[0].voltage) {
    /* Return with max capacity if voltage is greater than the max voltage in the model */
    res = model[0].capacity;
  } else if (voltage <= model[modelEntryCount - 1].voltage) {
    /* Return with min capacity if voltage is smaller than the min voltage in the model */
    res = model[modelEntryCount - 1].capacity;
  } else {
    uint8_t i;
    /* Otherwise find the 2 points in the model where the voltage level fits in between,
       and do linear interpolation to get the estimated capacity value */
    for (i = 0; i < (modelEntryCount - 1); i++) {
      if ((voltage < model[i].voltage) && (voltage >= model[i + 1].voltage)) {
        res = (uint8_t)((voltage - model[i + 1].voltage)
                        * (model[i].capacity - model[i + 1].capacity)
                        / (model[i].voltage - model[i + 1].voltage));
        res += model[i + 1].capacity;
        break;
      }
    }
  }

  return res;
}


// static uint8_t readBatteryLevel(void)
void readBatteryLevel(void)
{
  float voltage = SUPPLY_measureVoltage(1);
  battBatteryLevel = calculateLevel(voltage, battCR2032Model, sizeof(battCR2032Model) / sizeof(VoltageCapacityPair));
  // uint8_t level = calculateLevel(voltage, battCR2032Model, sizeof(battCR2032Model) / sizeof(VoltageCapacityPair));

  //return level;
}

#include "em_prs.h"

#define RX_OBS_PRS_CHANNEL 0
#define TX_OBS_PRS_CHANNEL 1

void enableDebugGpios(GPIO_Port_TypeDef rx_obs_port, uint8_t rx_obs_pin, GPIO_Port_TypeDef tx_obs_port, uint8_t tx_obs_pin)
{
  // Turn on the PRS and GPIO clocks to access their registers.
  // Configure pins as output.
  GPIO_PinModeSet(rx_obs_port, rx_obs_pin, gpioModePushPull, 0);
  GPIO_PinModeSet(tx_obs_port, tx_obs_pin, gpioModePushPull, 0);

  // Configure PRS Channel 0 to output RAC_RX.
  PRS_SourceAsyncSignalSet(0,PRS_ASYNC_CH_CTRL_SOURCESEL_RAC,_PRS_ASYNC_CH_CTRL_SIGSEL_RACLRX) ;
  PRS_PinOutput(RX_OBS_PRS_CHANNEL, prsTypeAsync,rx_obs_port, rx_obs_pin);

  /* Configure PRS Channel 0 to output RAC_RX.*/
  PRS_SourceAsyncSignalSet(1,PRS_ASYNC_CH_CTRL_SOURCESEL_RAC,_PRS_ASYNC_CH_CTRL_SIGSEL_RACLTX) ;
  PRS_PinOutput(TX_OBS_PRS_CHANNEL, prsTypeAsync,tx_obs_port, tx_obs_pin);
}
// enableDebugGpios(gpioPortF,6,gpioPortF,7);
