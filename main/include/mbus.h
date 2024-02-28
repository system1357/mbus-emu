#ifndef __MBUS_H
#define __MBUS_H
#include "esp_timer.h"

// config
#include "include/config.h"


#define BCD2INT(n) (((n)/16) * 10 + ((n) % 16)) 				// for max. 2 digits
#define INT2BCD(n) (((n)/10) * 16 + ((n) % 10)) 				// for max. 2 digits


#define MBUS_BUFFER		  32	// buffer size of m-bus packets

/* bitflags for content */
#define F_DISK   0x00000001
#define F_TRACK  0x00000002
#define F_INDEX  0x00000004
#define F_MINUTE 0x00000008
#define F_SECOND 0x00000010
#define F_FLAGS  0x00000020

/* bitflags for command state - Set play state (111xx) */
#define C_PLAY    0x00000001
#define C_PAUSE   0x00000002
#define C_PLAYFF  0x00000004
#define C_PLAYFR  0x00000008
#define C_SCNSTOP 0x00000010
#define C_UNUSED  0x00000020
#define C_STOP    0x00000040
#define C_RESUME  0x00000080

/* bitflags for play state - Set play state (99x...) */
#define P_PLAY      0x00000001
#define P_PAUSE     0x00000002
#define P_UNUSED    0x00000004
#define P_STOP      0x00000008
#define P_RANDOM    0x00000200
#define P_INTROSCN  0x00000800
#define P_REPEATONE 0x00004000
#define P_REPEATALL 0x00008000

/* bitflags for set program mode (Repeat and Mix) */
#define R_RANDOM     0x00000001
#define R_INTROSCN   0x00000002
#define R_REPEATONE  0x00000004
#define R_REPEATALL  0x00000008

// struct definitions
typedef struct {
  char message[32];
  volatile char rxbits[64]; 	
  volatile uint8_t num_bits;
  uint8_t len;
  uint8_t checksum;
} mbus_packet_t;


/* M-BUS source device */
typedef enum {
	eUnknown = 0,
	eRadio = 1,
	eCD = 9,
} source_t;


/* M-BUS commands */
typedef enum {
	eeInvalid,
	// radio to changer
	rPing,
	rPlay,
	rPause,
	rStop,
  rPowerdown,
	rScnStop,
	rPlayFF,
	rPlayFR,
	rPauseFF,
	rPauseFR,
	rResume,
	rResumeP,
	rNextMix,
	rPrevMix,
  rSkip,
	rRepeatOff,
	rRepeatOne,
	rRepeatAll,
	rScan,
	rMix,
	rSelect,
  rSelectDisk,
	rStatus,
	// changer to radio
	cPingOK,
	cAck,
	cPreparing,
	cStopped,
	cPaused,
	cPlaying,
	cSpinup,
	cForwarding,
	cReversing,
	cPwrUp,
	cLastInfo,
	cChanging,
	cChanging1,
	cChanging2,
	cChanging3,
	cChanging4,
	cNoMagzn,
	cStatus,
  cStatus1,
	cStat1,
	cStat2,
} command_t;


/* M-BUS packet type */
typedef struct {
	char acRaw[100]; 			// hex packet including checksum
	source_t source; 			// origin

	int chksum; 				// checksum
	int chksumOK; 				// checksum OK

	command_t cmd; 				// command ID
	const char *description; 	// decoded desciption
	int flagdigits; 			// how many flag digits
	uint16_t validcontent : 16;
	//DWORD validcontent; // bit flags validating the following information items

	int disk;   	// BCD format
	int track;  	// BCD format
	int index;  	// BCD format
	int minutes; 	// BCD format
	int seconds; 	// BCD format
	int flags;  	// BCD format
} mbus_data_t;

/* One entry in the coding table */
typedef struct
{	// one entry in the coding table
	command_t cmd;
	char hexmask[32];
	char infotext[32];
} code_item_t;

/* Command code table */
static const code_item_t alpine_codetable[] =
{
	{ rPing, 			  "18", 				      "Ping               " },
	{ cPingOK, 			"98", 				      "Ping OK            " },
	{ cAck, 			  "9F00fff", 			    "Ack/Wait           " }, 			// f0=0|1|6|7|9
	{ rStatus, 			"19",				        "Some info?         " },
	{ cPreparing,  	"991ttiimmssffff", 	"Preparing          " }, 			// f0=0:normal, f0=4:repeat one, f0=8:repeat all
	//{ cStopped,    	"992ttiimmssff0f", 	"Stopped            " }, 			// f1=0:normal, f1=2:mix, f1=8:scan
	{ cStopped,    	"992ttiimmssffff", 	"Stopped            " }, 			// f1=0:normal, f1=2:mix, f1=8:scan
	{ cPaused,     	"993ttiimmssff0f", 	"Paused             " }, 			// f3=1: play mode, f3=2:paused mode, f3=8: stopped
	{ cPlaying,    	"994ttiimmssff0f", 	"Playing            " },
	{ cSpinup,     	"995ttiimmssff0f", 	"Spinup             " },
	{ cForwarding, 	"996ttiimmssff0f", 	"FF                 " },
	{ cReversing,  	"997ttiimmssff0f", 	"FR                 " },
	{ rPlay,    		"11101", 			      "Play               " },
	{ rPause,   		"11102", 			      "Pause              " },
	{ rStop,    		"11140", 			      "Stop               " },
  { rPowerdown,		"11142", 			      "Powerdown          " },
	{ rScnStop, 		"11150", 			      "Scan Stop          " },
	{ rPlayFF,  		"11105", 			      "Play FF start      " },
	{ rPlayFR,  		"11109", 			      "Play FR start      " },
	{ rPauseFF, 		"11106", 			      "Pause FF start     " },
	{ rPauseFR, 		"1110A", 			      "Pause FR start     " },
	{ rResume,  		"11181", 			      "Play fr curr. pos. " },
	{ rResumeP, 		"11182", 			      "Pause fr curr. pos." },
	{ rNextMix, 	  "1130A31", 			    "next random" },
	{ rPrevMix, 	  "1130B31", 			    "previous random" },
  { rSkip,        "1130tt1",          "Skip Track"},
	{ rSelect,  		"113dttff", 		    "Select             " }, 			// f0=1:playing, f0=2:paused, f1=4:random
  { rSelectDisk,  "113d001",          "Select Disk        " },      // f=disk no 1-6
	{ rRepeatOff, 	"1140000", 		      "Repeat Off         " },
	{ rRepeatOne, 	"1144000", 		      "Repeat One         " },
	{ rRepeatAll, 	"11480000", 		    "Repeat All         " },
	{ rScan,      	"11408000", 		    "Scan               " },
	{ rMix,       	"1140200",  		    "Mix                " },
	{ cPwrUp, 			"9A0000000000", 	  "Some powerup?      " },
	{ cLastInfo,  	"9B0dttfff0f", 		  "Last played        " }, 		// f0=0:done, f0=1:busy, f0=8:eject, //f1=4: repeat1, f1=8:repeat all, f2=2:mix
	{ cNoMagzn,   	"9BAd00f00ff", 		  "No Magazin         " },
	//{ cChanging,  	"9B9dttfff0f", 		  "Changing           " },    //d=currentDisk
	{ cChanging,  	"9B9d0ttffff", 		  "Changing           " },    //d=currentDisk
	{ cChanging1, 	"9BDd00fff0f", 		  "UnloadDisk Changing Phase 1   " },
	{ cChanging2, 	"9BBd00fff0f", 		  "SelectDisk Changing Phase 2   " },    //D=disk
	{ cChanging3, 	"9BCd00fff0f", 		  "LoadDisk Phase 3   " },
	{ cChanging4, 	"9B8d00fff0f", 		  "CheckDisk Changing Phase 4   " },
	{ cStatus, 			"9Cd01ttmmssf", 	  "Disk Status        " },
  { cStatus1, 		"1Cd01ttmmssf", 	  "Disk Status1       " },
	{ cStat1, 			"9D000fffff", 		  "Some status?       " },
	{ cStat2, 			"9E0000000", 		    "Some more status?  " },
	{ eeInvalid, 		"0", 				        "Idle               " },
	// also seen:
	// 11191
};


enum ret_codes { 
	ok,
	reply, 
	end, 
	next, 
	fail,
	resume, 
	play, 
	stop, 
	changing
};

#define MAX_DYNAMIC_TIMERS 100
enum timerTypes {
	t_sendCmd,
	t_changeInterrupt
};

typedef struct {
  esp_timer_handle_t timerHandle;
  esp_timer_create_args_t timerArgs;
  char mbusPacket[MBUS_BUFFER];
  int dest;
  bool ready;
  int type;
  int data;
} dynamicTimers_t;


//static mbus_packet_t radioMbusTx;
//static mbus_packet_t changerMbusTx;

static const mbus_packet_t MBUSEMPTY;

// function declerations
// static void gpio_timer_readRadio(void* arg);
// static void gpio_timer_readChanger(void* arg);
// static void packetDone_timer_radio(void* arg);
// static void packetDone_timer_changer(void* arg);

//very fast routines routines
void IRAM_ATTR gpio_isr_handlerRadio(void* arg);
void IRAM_ATTR gpio_isr_handlerChanger(void* arg);
void IRAM_ATTR radioWriteBit();
void IRAM_ATTR changerWriteBit();
void IRAM_ATTR timer_cdcChanger(void* arg);
void IRAM_ATTR gpio_timer_readRadio(void* arg);
void IRAM_ATTR gpio_timer_readChanger(void* arg);
void IRAM_ATTR packetDone_timer_radio(void* arg);
void IRAM_ATTR packetDone_timer_changer(void* arg);
void IRAM_ATTR gpio_timer_writeRadio(void* arg);
void IRAM_ATTR gpio_timer_writeChanger(void* arg);
void IRAM_ATTR gpio_timer_writeNextBitRadio(void* arg);
void IRAM_ATTR gpio_timer_writeNextBitChanger(void* arg);
void IRAM_ATTR func_timerWriteNextCommandRadio(void* arg);
void IRAM_ATTR func_timerWriteNextCommandChanger(void* arg);

//test(char *mbusPacket, int time);

int8_t calc_checksum(char *buffer, uint8_t len);
int8_t calc_checksum2(mbus_packet_t *buffer);
uint8_t hex2int(char c);
char int2hex(uint8_t n);

uint8_t mbus_decode(mbus_data_t *mbuspacket, char *packet_src, size_t len);
uint8_t mbus_encode(mbus_data_t *mbuspacket, char *packet_dest);

void mbusSendRadio(char *mbusPacket);
void mbusSendChanger(char *mbusPacket);
void processCommand(mbus_data_t *mbuspacket, char *name);

void mbus_setup(void);

void disableInterrupt(int source);
void enableInterrupt(int source);
void sendTimedCmd(char *mbusPacket, int time, int dest);
void changeInterruptTimed(int dest, bool state, int time);
void setPassthrough(bool newMode);
void setPassthroughMode(bool newMode);

void mbus_debug(bool newState);


#endif
