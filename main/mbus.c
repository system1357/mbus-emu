/*
 * Written by William Peters in 2021
 *
 * 
 * This file contains some functions written by Harald W. Leschner (DK6YF), see mbusEmu.c:
 * mbus_decode
 * mbus_encode
 * hex2int
 * int2hex
 * calc_checksum
 */

// Platform generic includes
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

// application includes
#include "include/mbus.h"
#include "include/mbusEmu.h"

// queues
QueueHandle_t mbusRadioBitIn;
QueueHandle_t mbusChangerBitIn;
QueueHandle_t mbusRadioBitOut;
QueueHandle_t mbusChangerBitOut;

// timer vars

esp_timer_handle_t gpio_timer_readHandleRadio;
esp_timer_handle_t gpio_timer_readHandleChanger;
esp_timer_handle_t packetDone_timer_handleRadio;
esp_timer_handle_t packetDone_timer_handleChanger;
esp_timer_handle_t gpio_timer_writeHandleRadio;
esp_timer_handle_t gpio_timer_writeHandleChanger;
esp_timer_handle_t gpio_timer_writeNextBitHandleRadio;
esp_timer_handle_t gpio_timer_writeNextBitHandleChanger;
esp_timer_handle_t timerHandleWriteNextCommandRadio;
esp_timer_handle_t timerHandleWriteNextCommandChanger;

esp_timer_handle_t busFree_timer_handleRadio;
esp_timer_handle_t busFree_timer_handleChanger;

mbus_packet_t radioMbusRx;

bool mbusDebug=true;

char mbus_outbuffer[MBUS_BUFFER];	// global codec buffer for the driver
char mbus_inbuffer[MBUS_BUFFER];	// stores incoming message

// mutexes/spinlocks
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
volatile bool busBusyRadio=false;
volatile bool busSendingRadio=false;

char mbusRadioSendBuffer[MBUS_BUFFER*16];
volatile int mbusRadioSendBufferIndex;
volatile int mbusRadioSendBufferLen;

dynamicTimers_t dynamicTimers[MAX_DYNAMIC_TIMERS];

// gpio pin definitions
#define GPIO_OUTPUT_IO_0    CONFIG_PIN_MBUS_RADIO_OUT
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_IO_0)
#define GPIO_INPUT_IO_0     CONFIG_PIN_MBUS_RADIO_IN
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_INPUT_IO_0)

/*
 *  setup gpio/timers/int/tasks
 */
void mbus_setup(void) {
  printf("[MBUS] Initializing...\n");
  // Create queues
  printf("[MBUS] Creating queues...\t\t");
  mbusRadioBitIn = xQueueCreate(128, sizeof(uint32_t));
  printf("[ DONE ]\n");
  mbusRadioBitOut = xQueueCreate(512, sizeof(uint32_t));

  printf("[MBUS] Configuring GPIO...\t\t");
  // set direction of pins
  gpio_set_direction(GPIO_OUTPUT_IO_0, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_INPUT_IO_0, GPIO_MODE_INPUT);

  // set outputs to low
  gpio_set_level(GPIO_OUTPUT_IO_0, 0);
  printf("[ DONE ]\n");


  printf("[MBUS] Configuring interrupts...\t");
  // change gpio intrrupt type for one pin
  gpio_set_intr_type(CONFIG_PIN_MBUS_RADIO_IN, GPIO_INTR_ANYEDGE );

  //gpio_install_isr_service(ESP_INTR_FLAG_SHARED); //this is done in main
  gpio_isr_handler_add(CONFIG_PIN_MBUS_RADIO_IN, gpio_isr_handlerRadio, (void*) CONFIG_PIN_MBUS_RADIO_IN);

  gpio_intr_enable(CONFIG_PIN_MBUS_RADIO_IN);

  // readGPIORadio timer
  const esp_timer_create_args_t timer_readGPIORadio_args = {
        .callback = &gpio_timer_readRadio,
            /* argument specified here will be passed to timer callback function */
        .arg = (void*) gpio_timer_readHandleRadio, //Pass handle
        .name = "radio"
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_readGPIORadio_args, &gpio_timer_readHandleRadio));

  //packetDoneRadio Timer
  const esp_timer_create_args_t timer_packetDoneRadio_args = {
        .callback = &packetDone_timer_radio,
            /* argument specified here will be passed to timer callback function */
        .arg = (void*) packetDone_timer_handleRadio, //Pass handle
        .name = "radio"
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_packetDoneRadio_args, &packetDone_timer_handleRadio));

 // writeGPIORadio timer
  const esp_timer_create_args_t timer_writeGPIORadio_args = {
        .callback = &gpio_timer_writeRadio,
            /* argument specified here will be passed to timer callback function */
        .arg = (void*) gpio_timer_writeHandleRadio, //Pass handle
        .name = "radio"
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_writeGPIORadio_args, &gpio_timer_writeHandleRadio));

  const esp_timer_create_args_t timer_writeNextBitRadio_args = {
        .callback = &gpio_timer_writeNextBitRadio,
            /* argument specified here will be passed to timer callback function */
        .arg = (void*) gpio_timer_writeNextBitHandleRadio, //Pass handle
        .name = "radio"
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_writeNextBitRadio_args, &gpio_timer_writeNextBitHandleRadio));

  const esp_timer_create_args_t timerArgsWriteNextCommandRadio = {
    .callback = &func_timerWriteNextCommandRadio,
    .arg = (void*) timerHandleWriteNextCommandRadio, //Pass handle
    .name = "radio"
  };
  ESP_ERROR_CHECK(esp_timer_create(&timerArgsWriteNextCommandRadio, &timerHandleWriteNextCommandRadio));

  printf("[ DONE ]\n");
  mbusEmuInit();
}

/*
 *  gpio read timers
 */
void gpio_timer_readRadio(void* arg) {
  radioMbusRx.rxbits[radioMbusRx.num_bits] = gpio_get_level(CONFIG_PIN_MBUS_RADIO_IN);
  radioMbusRx.num_bits++;
}

/*
 *  packet Done timers
 */
void packetDone_timer_radio(void* arg) {
  busBusyRadio=false;
  memset(&radioMbusRx.message, '\0', sizeof(radioMbusRx.message));
  int nibbles=0;
  //printf("rxbits: ");
  for (int i=0; i<radioMbusRx.num_bits; i++) {
    //printf("%i", radioMbusRx.rxbits[i]);
    if((i % 4)==0) {
			uint8_t uHexDigit = 0;
			for (int j=0; j<4; j++) {
				if (radioMbusRx.rxbits[i+j] == 1)
					uHexDigit |= (1 << (3 - j));
				else if (radioMbusRx.rxbits[i+j] != 0)
					uHexDigit = 0xFF; // mark error
			}
      radioMbusRx.message[nibbles]=int2hex(uHexDigit);
      nibbles++;
    }
  }
  //printf("\n");
  radioMbusRx.len=nibbles-1;
  radioMbusRx.num_bits=0;
  radioMbusRx.checksum=calc_checksum2(&radioMbusRx);


  mbus_data_t newData;
  mbus_decode(&newData, radioMbusRx.message, radioMbusRx.len);
  radioMbusRx=MBUSEMPTY; //wipe struct
  char name[6]="radio";
  processCommand(&newData, name);
}

/*
 *  interrupt service routines
 *  keep a short time between changer and radio for collision protection?
 */

void handleTimedInterruptChange(void* arg) {
  dynamicTimers_t* pst = (dynamicTimers_t *)arg;
  printf("handleTimedInterruptChange dest:%i data:%i\n", pst->dest, pst->data);

  pst->ready=false;
  if(pst->data) {
    gpio_set_intr_type(CONFIG_PIN_MBUS_RADIO_IN, GPIO_INTR_ANYEDGE);
  } else {
    gpio_set_intr_type(CONFIG_PIN_MBUS_RADIO_IN, GPIO_INTR_DISABLE);
  }
  esp_timer_delete(pst->timerHandle); //clean up
}

void changeInterruptTimed(int dest, bool state, int time) {
  for (int i=0; i<MAX_DYNAMIC_TIMERS; i++) {
    if(!dynamicTimers[i].ready) {
      printf("CREATE TIMER %i: data:%i\n", i, state);
      esp_timer_create_args_t timerArg= {
        .callback = &handleTimedInterruptChange,
        .arg = (void*) &dynamicTimers[i],
        .name = "DYNAMIC_TASK"
        };
      dynamicTimers[i].timerArgs = timerArg;
      dynamicTimers[i].ready=true;
      dynamicTimers[i].dest=(int)dest;
      dynamicTimers[i].type=t_sendCmd;
      dynamicTimers[i].data=state;

      esp_timer_create(&dynamicTimers[i].timerArgs, &dynamicTimers[i].timerHandle);
      esp_timer_start_once(dynamicTimers[i].timerHandle, (time*1000));
      return;
    }
  }
}

void gpio_isr_handlerRadio(void* arg) {
  if(gpio_get_level(CONFIG_PIN_MBUS_RADIO_IN) == 1) {
    if(!busSendingRadio) {
      busBusyRadio=true;                                         //bus busy flag
      esp_timer_stop(packetDone_timer_handleRadio);              //reset packetDone timer
      esp_timer_start_once(gpio_timer_readHandleRadio, 800);    //readGPIO for current Radio mbus bit
      esp_timer_start_once(packetDone_timer_handleRadio, 4500);  //start packetDone timer
    }
  }
}

/*
 *  write timers
 */
void func_timerWriteNextCommandRadio(void* arg) {
  radioWriteBit();
}

void radioWriteBit() {
  uint8_t bit;
  portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
  if(busBusyRadio) {
    esp_timer_start_once(gpio_timer_writeNextBitHandleRadio, 100); //TESTTTT
    return;
  }
  if( xQueueReceiveFromISR( mbusRadioBitOut, &bit, &xHigherPriorityTaskWoken ) == pdTRUE ) {
    gpio_intr_disable(CONFIG_PIN_MBUS_RADIO_IN);
    busSendingRadio=true;
    //printf("%i", bit);
    if(bit==0) {
      esp_timer_start_once(gpio_timer_writeHandleRadio, 600);
    } else if (bit==1) {
      esp_timer_start_once(gpio_timer_writeHandleRadio, 1800);
    }

    if(bit == 9) { //this marks the end of packet
      esp_timer_start_once(gpio_timer_writeNextBitHandleRadio, 6000); //wait 6ms before sending next cmd
      busSendingRadio=false;
      gpio_intr_enable(CONFIG_PIN_MBUS_RADIO_IN);
    } else {
      esp_timer_start_once(gpio_timer_writeNextBitHandleRadio, 3000);
      gpio_set_level(CONFIG_PIN_MBUS_RADIO_OUT, 1);
    }
  }
}

/*
 *  gpio write timers
 */

void gpio_timer_writeRadio(void* arg) {
  portENTER_CRITICAL(&spinlock);                                // Lock port/gpio interrupts. Prevents racecondition
  gpio_set_level(CONFIG_PIN_MBUS_RADIO_OUT, 0);  // write pin
  portEXIT_CRITICAL(&spinlock);                                 // UnLock port/gpio interrupts
}

void gpio_timer_writeNextBitRadio(void* arg) {
  //busSendingRadio=false;
  radioWriteBit();
}

int8_t calc_checksum2(mbus_packet_t *buffer)
{
	int8_t checksum = 0;
	int8_t i;

	for (i = 0; i < buffer->len; i++) {
		checksum ^= buffer->message[i];
	}
	checksum = (checksum + 1) % 16;
  //printf("%x==%x  len:%i / %i ",checksum, hex2int(buffer->message[buffer->len]), buffer->len, buffer->len );
  if(hex2int(buffer->message[buffer->len])==checksum) {
    //printf("MATCH\n");
  } else {
    //printf("NOMATCHC\n");
  }
	return checksum;
}

int8_t calc_checksum(char *buffer, uint8_t len)
{
	int8_t checksum = 0;
	int8_t i;
  //printf("checksumcalc: ");
	for (i = 0; i < len; i++) {
    //printf("%X", hex2int(buffer[i]));
		checksum ^= hex2int(buffer[i]);
	}
  //printf("\n");
	checksum = (checksum + 1) % 16;

	return checksum;
}

uint8_t hex2int(char c) {
	if (c < '0')
		return 0xFF; 	// illegal hex digit
	else if (c <= '9')
		return (c - '0');
	else if (c < 'A')
		return 0xFF; 	// illegal hex digit
	else if (c <= 'F')
		return (c - 'A' + 10);
	else if (c < 'a')
		return 0xFF; 	// illegal hex digit
	else if (c <= 'f')
		return (c - 'a' + 10);

	return 0xFF; 		// default
}

char int2hex(uint8_t n) {
	if (n < 10)
		return ('0' + n);
	else if (n < 16)
		return ('A' + n - 10);

	return 'X'; 		// out of range
}

uint8_t mbus_decode(mbus_data_t *mbuspacket, char *packet_src, size_t len)
{
	//size_t len = strlen(packet_src);
	size_t i, j;
  //printf("DECODE pkt:%s len:%i", packet_src, len);
	// reset all the decoded information
 	memset(mbuspacket->acRaw, '\0', sizeof(mbuspacket->acRaw));
	memcpy(mbuspacket->acRaw, packet_src, len+1);
	mbuspacket->source = eUnknown;
	mbuspacket->chksum = -1;
	mbuspacket->chksumOK = false;
	mbuspacket->cmd = eeInvalid;
	mbuspacket->description = "";
	mbuspacket->flagdigits = 0;
	mbuspacket->validcontent = 0;
	mbuspacket->disk = 0;
	mbuspacket->track = 0;
	mbuspacket->index = 0;
	mbuspacket->minutes = 0;
	mbuspacket->seconds = 0;
	mbuspacket->flags = 0;

	if (len < 2)
		return 0xFF;

	//len--;	// remove last '\r'
	//len--;	// remove checksum
	mbuspacket->source = (source_t)(hex2int(packet_src[0])); // determine source from first digit
  //printf(" src:%i", mbuspacket->source);
	mbuspacket->chksum = calc_checksum(packet_src, len);
  //memcpy(mbuspacket->acRaw, calc_checksum(packet_src, len), 1);
	mbuspacket->chksumOK = (mbuspacket->chksum == hex2int(packet_src[len])); // verify checksum
  //printf(" checksum:%x", mbuspacket->chksum);
  //printf(" checksumOK:%i", mbuspacket->chksumOK);
	if (!mbuspacket->chksumOK) {
		//uart_write((uint8_t *)"?", 1);
		memset(&mbus_inbuffer, '\0', sizeof(mbus_inbuffer));	// delete content of inbuffer and start over
    //printf(" \n");
		//return 0xFF;
	}


	for (i = 0; i < sizeof(alpine_codetable) / sizeof(*alpine_codetable); i++) {
		// try all commands
		if (len != strlen(alpine_codetable[i].hexmask))
			continue; // size mismatch


		const char *src_ptr = packet_src; 						// source read pointer
		const char *cmp_ptr = alpine_codetable[i].hexmask; 		// current compare

		for (j = 0; j < len; j++) {
			// all (upper case) hex digits of the hexmask must match
			if ((*cmp_ptr >= '0' &&  *cmp_ptr <= '9') || (*cmp_ptr >= 'A' &&  *cmp_ptr <= 'F')) {

				if (*cmp_ptr != *src_ptr)
					break; // exit the char loop
			}
			src_ptr++;
			cmp_ptr++;
		}
    //printf(" cmdlen:%i/%i", j, len);
		if (j == len) {
			// a match, now decode parameters if present
			for (j = 0; j < len; j++) {

				switch (alpine_codetable[i].hexmask[j]) {

				case 'd': // disk
					mbuspacket->disk = (mbuspacket->disk << 4) | hex2int(packet_src[j]);
					mbuspacket->validcontent |= F_DISK;
					break;
				case 't': // track
					mbuspacket->track = (mbuspacket->track << 4) | hex2int(packet_src[j]);
					mbuspacket->validcontent |= F_TRACK;
					break;
				case 'i': // index
					mbuspacket->index = (mbuspacket->index << 4) | hex2int(packet_src[j]);
					mbuspacket->validcontent |= F_INDEX;
					break;
				case 'm': // minute
					mbuspacket->minutes = (mbuspacket->minutes << 4) | hex2int(packet_src[j]);
					mbuspacket->validcontent |= F_MINUTE;
					break;
				case 's': // second
					mbuspacket->seconds = (mbuspacket->seconds << 4) | hex2int(packet_src[j]);
					mbuspacket->validcontent |= F_SECOND;
					break;
				case 'f': // flags
					mbuspacket->flags = (mbuspacket->flags << 4) | hex2int(packet_src[j]);
					mbuspacket->validcontent |= F_FLAGS;
					mbuspacket->flagdigits++;
					break;
				} // switch

			} // for j

			mbuspacket->cmd = alpine_codetable[i].cmd;
			mbuspacket->description = alpine_codetable[i].infotext;
      //printf(" desc:%s", mbuspacket->description);
			if (mbuspacket->chksumOK) {
				memset(&mbus_inbuffer, '\0', sizeof(mbus_inbuffer));
			}
			break; // exit the command loop
		}
	}
	return (mbuspacket->cmd == eeInvalid) ? 0xFF : 0;
}


uint8_t mbus_encode(mbus_data_t *mbuspacket, char *packet_dest)
{
	uint8_t hr = 0;
	int8_t i,j;
	size_t len;
	mbus_data_t packet = *mbuspacket; // a copy which I can modify

	// seach the code table entry
	for (i = 0; i < sizeof(alpine_codetable) / sizeof(*alpine_codetable); i++) {
	 	// try all commands
		if (packet.cmd == alpine_codetable[i].cmd)
			break;
	}

	if (i == sizeof(alpine_codetable) / sizeof(*alpine_codetable)) {
		packet_dest = '\0'; 	// return an empty string
		return 0xFF; 			// not found
	}

	const char *pkt_template = alpine_codetable[i].hexmask;
	char *pkt_writeout = packet_dest;
	len = strlen(pkt_template);

	for (j = len - 1; j >= 0; j--) { // reverse order works better for multi-digit numbers

		if ((pkt_template[j] >= '0' &&  pkt_template[j] <= '9') || (pkt_template[j] >= 'A' &&  pkt_template[j] <= 'F')) {
			// copy regular hex digits
			pkt_writeout[j] = pkt_template[j];
			continue;
		}

		switch (pkt_template[j]) {
		 	// I just assume that any necessary parameter data is present
		case 'd': // disk
			pkt_writeout[j] = int2hex(packet.disk & 0x0F);
			packet.disk >>= 4;
			break;
		case 't': // track
			pkt_writeout[j] = int2hex(packet.track & 0x0F);
			packet.track >>= 4;
			break;
		case 'i': // index
			pkt_writeout[j] = int2hex(packet.index & 0x0F);
			packet.index >>= 4;
			break;
		case 'm': // minute
			pkt_writeout[j] = int2hex(packet.minutes & 0x0F);
			packet.minutes >>= 4;
			break;
		case 's': // second
			pkt_writeout[j] = int2hex(packet.seconds & 0x0F);
			packet.seconds >>= 4;
			break;
		case 'f': // flags
			pkt_writeout[j] = int2hex(packet.flags & 0x0F);
			packet.flags >>= 4;
			break;
		default: // unknow format char
			pkt_writeout[j] = '?';
			hr = 0x7F; // not quite OK
		}
	}


	int8_t checksum = calc_checksum(pkt_writeout, len);
	pkt_writeout[len] = int2hex(checksum); 	// add checksum
	pkt_writeout[len+1] = '\r'; 			// string termination
	pkt_writeout[len+2] = '\0'; 			// string termination

	return hr;
}

void processCommand(mbus_data_t *mbuspacket, char *name) { //emulator stuff and command extraction
  printf("%lu\tRXPKT BUS:%s SRC:%i DESC:%s raw:%s\n", xTaskGetTickCount(), name, mbuspacket->source, mbuspacket->description, mbuspacket->acRaw);
  emulatorMode(mbuspacket); //pass packet to emulator.
}

void mbusSendRadio(char *mbusPacket) {
  char y;
  for (int i=0; i<strlen(mbusPacket); i++) {
  	uint8_t z=hex2int(mbusPacket[i]);
  	for(int j=3; j>=0; j--) {
   		y=((z >> j)  & 0x01);
   		xQueueSend( mbusRadioBitOut, ( void * ) &y, ( TickType_t ) 0 );
  	}
  }
  y=9;
  xQueueSend( mbusRadioBitOut, ( void * ) &y, ( TickType_t ) 0 );
  esp_timer_start_once(gpio_timer_writeNextBitHandleRadio, 1);
}

void swek(void* arg){
  dynamicTimers_t* pst = (dynamicTimers_t *)arg;
  //printf("%u dest:%i cmd:%s\n",xTaskGetTickCount(), pst->dest, pst->mbusPacket);
  if(mbusDebug) printf("%lu\tTXPKT BUS:%i SRC:NA DESC:NA raw:%s\n", xTaskGetTickCount(), pst->dest, pst->mbusPacket);
  pst->ready=false;
  mbusSendRadio(pst->mbusPacket);
  esp_timer_delete(pst->timerHandle); //clean up
}

void createTimer(int type, esp_timer_cb_t callback, int data) { //todo create a wrapper with creation of dynamic timers
}

void sendTimedCmd(char *mbusPacket, int time, int dest) {
  for (int i=0; i<MAX_DYNAMIC_TIMERS; i++) {
    if(!dynamicTimers[i].ready) {
      if(mbusDebug) printf("CREATE TIMER %i: CMD:%s\n", i, mbusPacket);
      esp_timer_create_args_t timerArg= {
        .callback = &swek,
        .arg = (void*) &dynamicTimers[i],
        .name = "DYNAMIC_TASK"
        };
      dynamicTimers[i].timerArgs = timerArg;
      dynamicTimers[i].ready=true;
      dynamicTimers[i].dest=(int)dest;
      dynamicTimers[i].type=t_sendCmd;
      strcpy(dynamicTimers[i].mbusPacket, mbusPacket);
      //snprintf(dynamicTimers[i].mbusPacket, sizeof(dynamicTimers[i].mbusPacket), "%s", mbusPacket);
      //dynamicTimers[i].mbusPacket=mbusPacket;
      esp_timer_create(&dynamicTimers[i].timerArgs, &dynamicTimers[i].timerHandle);
      esp_timer_start_once(dynamicTimers[i].timerHandle, (time*1000));
      return;
    }
  }
}

void mbus_debug(bool newState) {
  mbusDebug=true;
}
