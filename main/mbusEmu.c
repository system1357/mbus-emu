/****************************************************************************
 * Copyright (C) 2016 by Harald W. Leschner (DK6YF)                         *
 * Adapted by William Peters in 2021										*
 *                                                                          *
 * This file is part of ALPINE M-BUS Interface Control Emulator             *
 *                                                                          *
 * This program is free software you can redistribute it and/or modify		*
 * it under the terms of the GNU General Public License as published by 	*
 * the Free Software Foundation either version 2 of the License, or 		*
 * (at your option) any later version. 										*
 *  																		*
 * This program is distributed in the hope that it will be useful, 			*
 * but WITHOUT ANY WARRANTY without even the implied warranty of 			*
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 			*
 * GNU General Public License for more details. 							*
 *  																		*
 * You should have received a copy of the GNU General Public License 		*
 * along with this program if not, write to the Free Software 				*
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA*
 ****************************************************************************/

// Platform generic includes
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include "sdkconfig.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


#include "include/mbusEmu.h"
#include "include/mbus.h"
#include "include/io.h"

#include "esp_timer.h"


bool emuDebug=true;

mbus_data_t in_packet;				// received state - decoded incoming packet : may be from radio or echo of our own

mbus_data_t response_packet;		// message state - to be sent to the head unit as response to another command

mbus_data_t status_packet;			// player state   - decoded outgoing packet : represents current player information

uint16_t player_sec = 0;

esp_timer_handle_t timer_cdcEmu_handle;

int (*cmd_funct)(void);
command_t cur_cmd = eeInvalid;
enum ret_codes rc;
command_t	last_radiocmd;
command_t	last_cdcmd;

bool radioOff=false;

/*
 ____           _ _
|  _ \ __ _  __| (_) ___
| |_) / _` |/ _` | |/ _ \
|  _ < (_| | (_| | | (_) |
|_| \_\__,_|\__,_|_|\___/

  ____                                          _
 / ___|___  _ __ ___  _ __ ___   __ _ _ __   __| |___
| |   / _ \| '_ ` _ \| '_ ` _ \ / _` | '_ \ / _` / __|
| |__| (_) | | | | | | | | | | | (_| | | | | (_| \__ \
 \____\___/|_| |_| |_|_| |_| |_|\__,_|_| |_|\__,_|___/
*/
/*
 * Perfom action for received command
 *
 * The next action is programmed by setting a new command and return with reply.
 * Otherwise, if return ok, no more messages are sent.
 */
static int idle_state(void) 
{
	response_packet = status_packet;

	response_packet.cmd = eeInvalid;
	status_packet.cmd   = eeInvalid;
    return ok;
}

static int ping_state(void) 
{
	response_packet.cmd = cPingOK;
    return reply;
}

static int play_state(void)
{
	printf("Play_state\n");
	response_packet = status_packet;

	response_packet.cmd = cPlaying;
	response_packet.flags &= ~0x00B;
	response_packet.flags |=  0x001;
    return reply;
}

static int pause_state(void)
{
	response_packet = status_packet;

	response_packet.cmd = cPaused;
	response_packet.flags &= ~0x00B;
	response_packet.flags |=  0x002;
    return reply;
}

static int stop_state(void)
{
	response_packet = status_packet;

	response_packet.cmd = cStopped;
	response_packet.flags |=  0x008;
    return reply;
}

static int scnstop_state(void)
{
    return ok;
}

static int playff_state(void)
{
    return ok;
}

static int playfr_state(void)
{
    return ok;
}

static int pauseff_state(void)
{
    return ok;
}

static int pausefr_state(void)
{
    return ok;
}

static int resume_state(void)
{
	response_packet = status_packet;

	response_packet.cmd = cChanging;
	response_packet.flags = 0x0001;
    return reply;
}

static int resumep_state(void)
{
	response_packet = status_packet;

	response_packet.cmd = cChanging;
	response_packet.flags = 0x0001;
    return reply;
}

static int nextmix_state(void) 
{
    return ok;
}

static int prevmix_state(void) 
{
    return ok;
}

static int repeatoff_state(void) 
{
	response_packet = status_packet;

	response_packet.flags &= ~0xCA0;

	status_packet.flags &= ~0xCA0;
    return ok;
}

static int repeatone_state(void) 
{
	response_packet = status_packet;

	response_packet.flags &= ~0xCA0;
	response_packet.flags |= 0x400;

	status_packet.flags &= ~0xCA0;
	status_packet.flags |=  0x400;
    return ok;
}

static int repeatall_state(void) 
{
	response_packet = status_packet;

	response_packet.flags &= ~0xCA0;
	response_packet.flags |= 0x800;

	status_packet.flags &= ~0xCA0;
	status_packet.flags |=  0x800;
    return ok;
}

static int scan_state(void) 
{
	response_packet = status_packet;

	response_packet.flags &= ~0xCA0;
	response_packet.flags |= 0x080;

	status_packet.flags &= ~0xCA0;
	status_packet.flags |=  0x080;
    return ok;
}

static int mix_state(void) 
{
	response_packet = status_packet;

	response_packet.flags &= ~0xCA0;
	response_packet.flags |= 0x020;

	status_packet.flags &= ~0xCA0;
	status_packet.flags |=  0x020;
    return ok;
}

static int select_state(void) 
{
	response_packet.cmd = cChanging;

	if (in_packet.disk != 0 && in_packet.disk != status_packet.disk) {
		response_packet.disk = in_packet.disk;
		//status_packet.disk = 9; // test hack!!
		response_packet.track = 1;
		response_packet.flags = 0x1001; // busy

		status_packet.track = 1;
		status_packet.disk = in_packet.disk;
	} else {
		// zero indicates same disk
		response_packet.disk = status_packet.disk;
		response_packet.track = in_packet.track;
		response_packet.flags = 0x0001; // done
		
		status_packet.track = in_packet.track;
	}

	status_packet.minutes = 0;
	status_packet.seconds = 0;
	player_sec = 0; // restart timer
    return reply;
}

static int status_state(void) 
{
	response_packet.cmd = cAck;
    return reply;
}



/*
  ____ _                                 
 / ___| |__   __ _ _ __   __ _  ___ _ __ 
| |   | '_ \ / _` | '_ \ / _` |/ _ \ '__|
| |___| | | | (_| | | | | (_| |  __/ |   
 \____|_| |_|\__,_|_| |_|\__, |\___|_|   
                         |___/           
  ____                                          _     
 / ___|___  _ __ ___  _ __ ___   __ _ _ __   __| |___ 
| |   / _ \| '_ ` _ \| '_ ` _ \ / _` | '_ \ / _` / __|
| |__| (_) | | | | | | | | | | | (_| | | | | (_| \__ \
 \____\___/|_| |_| |_|_| |_| |_|\__,_|_| |_|\__,_|___/
*/
/*  
 * If we send out a reply to a command, the receiver decodes it like any other one...
 * This "echo" can be used to trigger a new command coming from our emulator.
 *
 * The next action is programmed by setting a new command and return with reply.
 * Otherwise, if return ok, no more messages are sent.
 */
static int ping_self(void) 
{
    return ok;
}

static int ack_self(void) 
{
	// ToDo: Prevent loop with cStatus replying with cAck
	//if (response_packet.flags & C_UNUSED) {
	//	response_packet.cmd = eInvalid;
	//	response_packet.flags &= ~ C_UNUSED;
	//	return ok;
	//}

	/* After changing we switch to play mode */
	if (last_radiocmd == rSelect) {
		response_packet.cmd = cPlaying;
		response_packet.flags = 0x001;
		
		status_packet.cmd = cPlaying;
		status_packet.flags = 0x001;
		return reply;

	/* After resuming we have to send a Disc Status update */
	} else if (last_radiocmd == rResume) {
		response_packet.cmd = cStatus;
		response_packet.flags &= ~0x00B;
		response_packet.flags |=  0x001;

	} else if (last_radiocmd == rResumeP) {
		response_packet.cmd = cStatus;
		response_packet.flags &= ~0x00B;
		response_packet.flags |=  0x002;

	/* After power-up we have to send a Disc Status update */
	} else if (last_radiocmd == rStatus) {
		response_packet.cmd = cStatus;

	} else
		response_packet.cmd = cAck;

	if (response_packet.cmd == cStatus) {
		response_packet.disk = status_packet.disk;
		response_packet.track = INT2BCD(99);
		response_packet.minutes = INT2BCD(99);
		response_packet.seconds = INT2BCD(99);
		return reply;
	}

	return ok;
}

static int preparing_self(void) 
{
    return ok;
}

static int stopped_self(void) 
{
	status_packet.cmd = cStopped;
	//status_packet.flags &= ~0x00B;  // must not delete previous play/pause state, just add stop
	status_packet.flags |=  0x008;
    return ok;
}

static int paused_self(void) 
{
	status_packet.cmd = cPaused;
	status_packet.flags &= ~0x00B;
	status_packet.flags |=  0x002;
    return ok;
}

static int playing_self(void) 
{
	status_packet.cmd = cPlaying;
	status_packet.flags &= ~0x00B;
	status_packet.flags |=  0x001;
    return ok;
}

static int spinup_self(void) 
{
    return ok;
}

static int forwarding_self(void) 
{
    return ok;
}

static int reversing_self(void) 
{
    return ok;
}

static int powerup_self(void) 
{
    return ok;
}

static int lastinfo_self(void) 
{
    return ok;
}

static int changing_self(void) 
{
	response_packet.cmd = cAck;
	response_packet.flags = 0x0001; // done
    return reply;
}

static int changing1_self(void) 
{
    return ok;
}

static int changing2_self(void) 
{
    return ok;
}

static int changing3_self(void) 
{
    return ok;
}

static int changing4_self(void) 
{
    return ok;
}

static int nomagazin_self(void) 
{
    return ok;
}

static int status_self(void) 
{
	//response_packet.cmd = cAck;
    //response_packet.flags |= C_UNUSED;	// ToDo Perevent loop with cAck
    //return reply;
    return ok;
}

static int status11_self(void) 
{
	//response_packet.cmd = cAck;
    //response_packet.flags |= C_UNUSED;	// ToDo Perevent loop with cAck
    //return reply;
    return ok;
}

static int status1_self(void) 
{
    return ok;
}

static int status2_self(void) 
{
    return ok;
}

int (*cmd_state[])(void) = {
	idle_state,
	// Radio to Changer 
	ping_state,
	play_state,
	pause_state,
	stop_state, 
	scnstop_state, 
	playff_state, 
	playfr_state, 
	pauseff_state, 
	pausefr_state,
	resume_state,
	resumep_state,
	nextmix_state,
	prevmix_state,
	repeatoff_state,
	repeatone_state,
	repeatall_state,
	scan_state,
	mix_state,
	select_state,
	status_state,
	// Changer to Radio
	ping_self,
	ack_self,
	preparing_self,
	stopped_self,
	paused_self,
	playing_self,
	spinup_self,
	forwarding_self,
	reversing_self,
	powerup_self,
	lastinfo_self,
	changing_self, 
	changing1_self, 
	changing2_self, 
	changing3_self, 
	changing4_self,
	nomagazin_self,
	status_self,
	status11_self,
	status1_self,
	status2_self,
};

bool emuEnabled=0;
int emuMode=0;
int emuModeCount=0;





int curDisk=0;
int curDiskTracks=0;
int curDiskTimeMin=0;
int curDiskTimeSec=0;
int curIndex=0;
int curTrack=0;
int curMin=0;
int curSec=0;
uint32_t curFlags=0;

void switchEmuCD(int diskNum) {
	mbus_data_t tmpPacket;
	char localmbus_outbuffer[MBUS_BUFFER];
	char ackData[MBUS_BUFFER];

	//9F005561 //disk1
	//9F007561 //disk2
	//9F009461 //disk3
	//9F00B361 //disk4
	//9F00D261 //disk5
	//9F00F261 //disk6
	if(diskNum==1) {
		strcpy(ackData, "9F005561");
	} else if(diskNum==2) {
		strcpy(ackData, "9F007561");
	} else if(diskNum==3) {
		strcpy(ackData, "9F009461");
	} else if(diskNum==4) {
		strcpy(ackData, "9F00B361");
	} else if(diskNum==5) {
		strcpy(ackData, "9F00D261");
	} else if(diskNum==6) {
		strcpy(ackData, "9F00F261");
	}

	tmpPacket.disk=diskNum;
	tmpPacket.track=INT2BCD(curTrack);
	tmpPacket.index=INT2BCD(curIndex);
    tmpPacket.minutes = INT2BCD(0);
	tmpPacket.seconds = INT2BCD(0);
	tmpPacket.flags = curFlags;

	esp_timer_stop(timer_cdcEmu_handle);

	tmpPacket.cmd=cChanging;
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	printf("SEND CD cChanging PACKET:%i d:%d t:%i i:%i m:%i s:%i flags:%u cmd:%s\n", tmpPacket.cmd, tmpPacket.disk, BCD2INT(tmpPacket.track), BCD2INT(response_packet.index), BCD2INT(tmpPacket.minutes), BCD2INT(tmpPacket.seconds), tmpPacket.flags, localmbus_outbuffer);
    sendTimedCmd(localmbus_outbuffer, 50, eRadio);

	tmpPacket.cmd=cStopped;
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	printf("SEND CD cStopped PACKET:%i d:%d t:%i i:%i m:%i s:%i flags:%u cmd:%s\n", tmpPacket.cmd, tmpPacket.disk, BCD2INT(tmpPacket.track), BCD2INT(response_packet.index), BCD2INT(tmpPacket.minutes), BCD2INT(tmpPacket.seconds), tmpPacket.flags, localmbus_outbuffer);
    sendTimedCmd(localmbus_outbuffer, 250, eRadio);

	tmpPacket.cmd=cChanging1;
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	printf("SEND CD cChanging1 PACKET:%i d:%d t:%i i:%i m:%i s:%i flags:%u cmd:%s\n", tmpPacket.cmd, tmpPacket.disk, BCD2INT(tmpPacket.track), BCD2INT(response_packet.index), BCD2INT(tmpPacket.minutes), BCD2INT(tmpPacket.seconds), tmpPacket.flags, localmbus_outbuffer);
    sendTimedCmd(localmbus_outbuffer, 450, eRadio);
	
	tmpPacket.track=0;
	tmpPacket.cmd=cPreparing;
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	printf("SEND CD cPreparing PACKET:%i d:%d t:%i i:%i m:%i s:%i flags:%u cmd:%s\n", tmpPacket.cmd, tmpPacket.disk, BCD2INT(tmpPacket.track), BCD2INT(response_packet.index), BCD2INT(tmpPacket.minutes), BCD2INT(tmpPacket.seconds), tmpPacket.flags, localmbus_outbuffer);
    sendTimedCmd(localmbus_outbuffer, 550, eRadio);


	sendTimedCmd(ackData, 650, eRadio);

	tmpPacket.track=INT2BCD(80);
	tmpPacket.minutes=INT2BCD(80);
	tmpPacket.cmd=cStatus;
    mbus_encode(&tmpPacket, localmbus_outbuffer);
	printf("SEND CD cStatus PACKET:%i d:%d t:%i i:%i m:%i s:%i flags:%u cmd:%s\n", tmpPacket.cmd, tmpPacket.disk, BCD2INT(tmpPacket.track), BCD2INT(response_packet.index), BCD2INT(tmpPacket.minutes), BCD2INT(tmpPacket.seconds), tmpPacket.flags, localmbus_outbuffer);
    sendTimedCmd(localmbus_outbuffer, 750, eRadio);

	// //cPwrUp
	// 9F005561
	// sendTimedCmd("9F005561", 650, eRadio);

	// tmpPacket.flags = 7;
	// tmpPacket.track=INT2BCD(50);
	// tmpPacket.cmd=cStatus;
    // mbus_encode(&tmpPacket, localmbus_outbuffer);
	// printf("SEND CD cStatus PACKET:%i d:%d t:%i i:%i m:%i s:%i flags:%u cmd:%s\n", tmpPacket.cmd, tmpPacket.disk, BCD2INT(tmpPacket.track), BCD2INT(response_packet.index), BCD2INT(tmpPacket.minutes), BCD2INT(tmpPacket.seconds), tmpPacket.flags, localmbus_outbuffer);
    // sendTimedCmd(localmbus_outbuffer, 650, eRadio);

//somestatus
	tmpPacket.flags = 0;
	tmpPacket.cmd=cStat1;
    mbus_encode(&tmpPacket, localmbus_outbuffer);
	printf("SEND CD cStat1 PACKET:%i d:%d t:%i i:%i m:%i s:%i flags:%u cmd:%s\n", tmpPacket.cmd, tmpPacket.disk, BCD2INT(tmpPacket.track), BCD2INT(response_packet.index), BCD2INT(tmpPacket.minutes), BCD2INT(tmpPacket.seconds), tmpPacket.flags, localmbus_outbuffer);
    sendTimedCmd(localmbus_outbuffer, 825, eRadio);

// 	cSpinup
// 9950101000000015
	tmpPacket.flags = 0x001;
	tmpPacket.track=1;
	tmpPacket.minutes=0;
	tmpPacket.cmd=cSpinup;
    mbus_encode(&tmpPacket, localmbus_outbuffer);
	printf("SEND CD cSpinup PACKET:%i d:%d t:%i i:%i m:%i s:%i flags:%u cmd:%s\n", tmpPacket.cmd, tmpPacket.disk, BCD2INT(tmpPacket.track), BCD2INT(response_packet.index), BCD2INT(tmpPacket.minutes), BCD2INT(tmpPacket.seconds), tmpPacket.flags, localmbus_outbuffer);
    sendTimedCmd(localmbus_outbuffer, 900, eRadio);

	sendTimedCmd(ackData, 975, eRadio);

	curTrack=1;
	esp_timer_start_periodic(timer_cdcEmu_handle, 1000000);
}


bool powerDownInProgess=false;
bool emulatorReady=true;
unsigned int emulatorBusyTime=0;

void setEmulatorBusy(int time) {
	emulatorBusyTime = xTaskGetTickCount()+time;
	emulatorReady=false;
}



void emulatorMode(const mbus_data_t *inpacket) {
	mbus_data_t tmpPacket;
	char localmbus_outbuffer[MBUS_BUFFER];
	if(inpacket->source==9) { //ignore CDC for now.
		if(inpacket->cmd == cPlaying) {
			printf("Disk: %i Track: %i Index: %i Minutes: %i Seconds: %i\n", BCD2INT(inpacket->disk), BCD2INT(inpacket->track), BCD2INT(inpacket->index), BCD2INT(inpacket->minutes), BCD2INT(inpacket->seconds));
			//curDisk=inpacket->disk;
			curTrack=BCD2INT(inpacket->track);
			curMin=inpacket->minutes;
			curSec=inpacket->seconds;
			curIndex=inpacket->index;
			curFlags=inpacket->flags;
			player_sec=inpacket->seconds+(curMin*60);
		} else if(inpacket->cmd == cStatus) {
			printf("DiskStatus Disk:%i Track:%i Min:%i Sec:%i Flag:%i\n", BCD2INT(inpacket->disk), BCD2INT(inpacket->track), BCD2INT(inpacket->minutes), BCD2INT(inpacket->seconds), inpacket->flags);
			curDisk=inpacket->disk; //only here the "currentdisk" is sent by the cdc
			curDiskTracks=inpacket->track;
			curDiskTimeMin=inpacket->minutes;
			curDiskTimeSec=inpacket->seconds;
		} else {
			return;
		}
	}

	if(!emuEnabled) { //end function if passthrough
		return;
	}

	if(emulatorBusyTime < xTaskGetTickCount()) { //end if emulator is busy sending an array of commands
		emulatorReady=true;
	}

  	printf("EmuPkt! for %s from %i\n", inpacket->description, inpacket->source);
  	cur_cmd = inpacket->cmd;
  	if (inpacket->source == eRadio) {
		last_radiocmd = cur_cmd;
  	}
  	cmd_funct = cmd_state[cur_cmd];
    /* Execute function assigned for reception */
    rc = cmd_funct(); //if this fails, a command was not found.

  	// if (ok == rc) {
    // 	return;
	// }
	// Handle basic commands
	if(inpacket->cmd == rPing) {
		sendTimedCmd("98A", 50, eRadio);
	} else if(inpacket->cmd ==cAck) {
		printf("cAck PACKET:%i flags:%u\n", tmpPacket.cmd, tmpPacket.flags);
	} else if(inpacket->cmd ==rPowerdown) { // handle shutdown commands in emumode
		if(powerDownInProgess) {
			return;
		}
		powerDownInProgess=true;
		esp_timer_stop(timer_cdcEmu_handle);
		radioOff=true;
		tmpPacket.cmd=cStopped;
		tmpPacket.disk=INT2BCD(curDisk);
		tmpPacket.track=INT2BCD(curTrack); //convert magic to BCD
		tmpPacket.index=INT2BCD(curIndex);
		tmpPacket.minutes = INT2BCD(curMin);
		tmpPacket.seconds = INT2BCD(curSec);
		tmpPacket.flags = 0xA;

		sendTimedCmd("9F00946E", 100, eRadio);

    	mbus_encode(&tmpPacket, localmbus_outbuffer);
		char stoppedCmd[MBUS_BUFFER];
		strcpy(stoppedCmd, localmbus_outbuffer);
		printf("Stopped PACKET:%i d:%d t:%i i:%i m:%i s:%i flags:%u cmd:%s\n", tmpPacket.cmd, tmpPacket.disk, tmpPacket.track, response_packet.index, tmpPacket.minutes, tmpPacket.seconds, tmpPacket.flags, localmbus_outbuffer);
		sendTimedCmd(stoppedCmd, 400, eRadio);
		sendTimedCmd(stoppedCmd, 600, eRadio);
		sendTimedCmd(stoppedCmd, 800, eRadio);
		sendTimedCmd("9F00946E", 900, eRadio);

		tmpPacket.cmd=cStatus;
		mbus_encode(&tmpPacket, localmbus_outbuffer);
		sendTimedCmd(localmbus_outbuffer, 1100, eRadio);

		tmpPacket.cmd=cStat1;
		mbus_encode(&tmpPacket, localmbus_outbuffer);
		sendTimedCmd(localmbus_outbuffer, 1200, eRadio);

		sendTimedCmd(stoppedCmd, 1300, eRadio);
		

	} else if(inpacket->cmd == rPlay && emulatorReady) {
		if(powerDownInProgess){
			powerDownInProgess=false;
			esp_timer_start_periodic(timer_cdcEmu_handle, 1000000);
		} else {
			tmpPacket.cmd=cAck;
			mbus_encode(&tmpPacket, localmbus_outbuffer);
			//sendTimedCmd(localmbus_outbuffer, 0, eRadio); // ACK/WAIT
			//sendTimedCmd(localmbus_outbuffer, 50, eRadio); // ACK/WAIT
		}
	// 	tmpPacket.cmd=cPreparing;
	// 	tmpPacket.track=0;
	// 	tmpPacket.index=0;
	// 	tmpPacket.minutes = 0;
	// 	tmpPacket.seconds = 0;
	// 	tmpPacket.flags = 0x001;
    // 	mbus_encode(&tmpPacket, localmbus_outbuffer);
	// 	printf("cPreparing PACKET:%i d:%d t:%i i:%i m:%i s:%i flags:%u cmd:%s\n", tmpPacket.cmd, tmpPacket.disk, tmpPacket.track, response_packet.index, tmpPacket.minutes, tmpPacket.seconds, tmpPacket.flags, localmbus_outbuffer);
    // 	//sendTimedCmd(localmbus_outbuffer, 0, eRadio);
	// 	sendTimedCmd(localmbus_outbuffer, 50, eRadio);
	} else if(inpacket->cmd == rSkip) {
		printf("SkipTrack: %i\n", BCD2INT(inpacket->track));
		int cTrackNum=BCD2INT(inpacket->track);
		if(cTrackNum==0) { 
			cTrackNum=1;
		}
		curTrack=cTrackNum;
		player_sec=0;
	} else if(inpacket->cmd == rRepeatOff) {
		curFlags &= ~0xCA0;
	} else if(inpacket->cmd == rRepeatOne) {
		curFlags &= ~0xCA0;
		curFlags |=  0x400;
	} else if(inpacket->cmd == rRepeatAll) {
		curFlags &= ~0xCA0;
		curFlags |=  0x800;
	} else if(inpacket->cmd == rSelectDisk && emulatorReady) {
		printf("SelectDisk: %i\n", inpacket->disk);
		//switchEmuCD(BCD2INT(inpacket->disk));
		rLoadDisk(BCD2INT(inpacket->disk));

	} else if(inpacket->cmd == rStatus && emulatorReady) { //this packet is requested when the radio loses sync with cdc? Resend current EMU status
		sendTimedCmd("810A", 50, eRadio); // 810A?

		sendTimedCmd("9F00B369", 150, eRadio); // ACK

		tmpPacket.flags &= ~0xCA0;
		tmpPacket.flags |= 0x007;
		tmpPacket.cmd=cStatus;
		tmpPacket.track=INT2BCD(98); //indicate max tracks for special mode
		tmpPacket.minutes = INT2BCD(74);
		tmpPacket.seconds = INT2BCD(55);
		tmpPacket.disk=INT2BCD(curDisk);
		mbus_encode(&tmpPacket, localmbus_outbuffer);
		sendTimedCmd(localmbus_outbuffer, 500, eRadio);

		sendTimedCmd("9A00000000004", 700, eRadio);
		sendTimedCmd("9D000000005", 900, eRadio);
		tmpPacket.cmd=cChanging;
		tmpPacket.flags=0x001;
		tmpPacket.track=INT2BCD(0);
		mbus_encode(&tmpPacket, localmbus_outbuffer);
		sendTimedCmd(localmbus_outbuffer, 1000, eRadio);
		//sendTimedCmd("9B9401000010", 1000, eRadio);
		
		
	} else if(emuMode==0) { //pretend to eject mag

	} else {
	// if (ok == rc) {
    // 	return;
	// }
	if (reply == rc) {
		if (inpacket->source == eRadio) { //process only radio
			//char localmbus_outbuffer[MBUS_BUFFER];
    		mbus_encode(&response_packet, localmbus_outbuffer);
    		sendTimedCmd(localmbus_outbuffer, 0, eRadio);

			printf("SEND RESPONSE: SRC: %i CMD: %s\n", inpacket->source, localmbus_outbuffer);
    		// char y;
    		// for (int i=0; i<strlen(mbus_outbuffer)-1; i++) {
      		// 	char z=mbus_outbuffer[i];
      		// 	for(int j=3; j>=0; j--) {
        	// 		y=((z >> j)  & 0x01);
        	// 		xQueueSend( mbusRadioBitOut, ( void * ) &y, ( TickType_t ) 0 );
      		// 	}
    		// }
    		// y=9;
    		// xQueueSend( mbusRadioBitOut, ( void * ) &y, ( TickType_t ) 0 );
    		// esp_timer_start_once(gpio_timer_writeNextBitHandleRadio, 10);
		}
    }
	}
}

void timer_cdcChanger(void* arg) { //pretend to be playing. send repeating packet with same seconds
	mbus_data_t tmpPacket;
	//if (status_packet.cmd == cPlaying)
    	player_sec++;

  	if (player_sec >= 5400)   // after 90 minutes reset the counter
    	player_sec = 0;
	//curTrack++;

	tmpPacket.cmd=cPlaying;
	tmpPacket.disk=curDisk;
	tmpPacket.track=INT2BCD(curTrack); //convert magic to BCD
	tmpPacket.index=curIndex;
    tmpPacket.minutes = INT2BCD(player_sec / 60);
	tmpPacket.seconds = INT2BCD(player_sec % 60);
	//tmpPacket.minutes = curMin;
	//tmpPacket.seconds = curSec;
	tmpPacket.flags = curFlags;

	char localmbus_outbuffer[MBUS_BUFFER];
    mbus_encode(&tmpPacket, localmbus_outbuffer);
	if(emuDebug) printf("STATUS PACKET:%i d:%d t:%i i:%i m:%i s:%i flags:%u cmd:%s\n", tmpPacket.cmd, tmpPacket.disk, tmpPacket.track, response_packet.index, tmpPacket.minutes, tmpPacket.seconds, tmpPacket.flags, localmbus_outbuffer);
    sendTimedCmd(localmbus_outbuffer, 0, eRadio);
}

int preEmuDisk=0;
int preEmuTrack=0;
int preEmuNumTracks=0;
int preEmuIndex=0;
int preEmuTotalMin=0;
int preEmuTotalSec=0;
int preEmuMin=0;
int preEmuSec=0;


void deactivateEmulationMode() { //fool the radio unload and reload "previous disk" to "continue"
	setEmulatorBusy(1200);
	mbus_data_t tmpPacket;
	char localmbus_outbuffer[MBUS_BUFFER];
	tmpPacket.disk=INT2BCD(curDisk);
	tmpPacket.track=INT2BCD(curDiskTracks); //convert magic to BCD
	//tmpPacket.index=curIndex;
    //response_packet.minutes = INT2BCD(player_sec / 60);
	//response_packet.seconds = INT2BCD(player_sec % 60);
	tmpPacket.minutes = INT2BCD(curDiskTimeMin);
	tmpPacket.seconds = INT2BCD(curDiskTimeSec);
	tmpPacket.flags = curFlags;

	esp_timer_stop(timer_cdcEmu_handle);

	tmpPacket.cmd = rPlay;
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	sendTimedCmd(localmbus_outbuffer, 900, eCD); //send Play
	sendTimedCmd(localmbus_outbuffer, 1000, eCD); //send Play
	changeInterruptTimed(eCD, true, 1100); //Enable cdc interrupts after 600ms (removes extra garbage from cdc bus)
	curTrack=preEmuTrack;
	rLoadDisk(preEmuDisk);
	io_switchInput(inCDC);
}

void activateEmulationMode(int mode) {
	setEmulatorBusy(100);
	mbus_data_t tmpPacket;
  	char localmbus_outbuffer[MBUS_BUFFER];
	preEmuDisk=INT2BCD(curDisk);
	preEmuTrack=INT2BCD(curTrack);
	preEmuNumTracks=INT2BCD(curDiskTracks);
	preEmuIndex=INT2BCD(curIndex);
	preEmuTotalMin=INT2BCD(curDiskTimeMin);
	preEmuTotalSec=INT2BCD(curDiskTimeSec);
	preEmuMin=INT2BCD(curMin);
	preEmuSec=INT2BCD(curSec);

	disableInterrupt(eCD);
	rLoadDisk(mode);

	printf("Activate emu! D:%i t:%i i:%i m:%i s:%i tt:%i tm:%i ts:%i\n",
		BCD2INT(preEmuDisk), BCD2INT(preEmuTrack), BCD2INT(preEmuIndex), BCD2INT(preEmuMin), BCD2INT(preEmuSec), 
		BCD2INT(preEmuNumTracks), BCD2INT(preEmuTotalMin), BCD2INT(preEmuTotalSec));
	emuMode=0;
	emuModeCount=0;
	esp_timer_start_periodic(timer_cdcEmu_handle, 1000000); //update playback state every second.

	tmpPacket.cmd = rPowerdown;
    mbus_encode(&tmpPacket, localmbus_outbuffer);
	sendTimedCmd(localmbus_outbuffer, 100, eCD); //send Play
	sendTimedCmd(localmbus_outbuffer, 50, eCD); //send Play

}

void mbusEmuModeChange(bool newState) {
  emuEnabled=newState;
  if(newState) { //start emu mode
	activateEmulationMode(1);	//trigger events required for emulationMode
  } else { //stop emu mode
	deactivateEmulationMode(); //try to resume playback
  }
}

void mbusEmuInit() {
	printf("[MBUSEMU] Initializing...\t\t");
	const esp_timer_create_args_t timer_cdcEmu_args = {
        .callback = &timer_cdcChanger,
            /* argument specified here will be passed to timer callback function */
        .arg = (void*) timer_cdcEmu_handle, //Pass handle 
        .name = "radio"
  	};
  	ESP_ERROR_CHECK(esp_timer_create(&timer_cdcEmu_args, &timer_cdcEmu_handle));
	printf("[ DONE ]\n");
}



// start/stop sending cPlaying packets.
void cdcEmuPcktDisable() {
	esp_timer_stop(timer_cdcEmu_handle);
}

void cdcEmuPcktEnable() {
	esp_timer_start_periodic(timer_cdcEmu_handle, 1000000);
}

// Some basic CDC commands

void cdcChangeEjectMag() { //fake radio we ejected our magazine (actual packets snooped from the bus)
	cdcEmuPcktDisable();
	setEmulatorBusy(2000);
	sendTimedCmd("9B9606800013", 50, eRadio); // changing
	sendTimedCmd("9F00F26E", 150, eRadio); // ACK/WAIT
	sendTimedCmd("9920001000000013", 350, eRadio); // STOPPED
	sendTimedCmd("99200010000000AA", 650, eRadio); // STOPPED
	sendTimedCmd("9F00C26F", 850, eRadio); // ACK/WAIT
	sendTimedCmd("9BD6008000AC", 1050, eRadio); // cChanging1
	sendTimedCmd("9BD6008000AC", 1150, eRadio); // cChanging1
	sendTimedCmd("9F00C25E", 1350, eRadio); // ACK/WAIT
	sendTimedCmd("9B86008000AF", 1450, eRadio); // cChanging4
	sendTimedCmd("9F000B1D", 1550, eRadio); // ACK/WAIT
	sendTimedCmd("9BA6008000AD", 1650, eRadio); // no mag
	sendTimedCmd("9F00CD17", 1750, eRadio); // ACK/WAIT
	sendTimedCmd("9BA1002002A4", 1750, eRadio); // no mag
	sendTimedCmd("99200010000002AC", 1850, eRadio); // no mag
	sendTimedCmd("9BA1002000A2", 1950, eRadio); // no mag
	sendTimedCmd("9BA1002000A2", 1999, eRadio); // no mag
}

void cdcChangeInsertMag() { //fake radio we ejected our magazine (actual packets snooped from the bus)
	setEmulatorBusy(2000);
	curDisk=1;
	curTrack=1;
	sendTimedCmd("9BA1000000A4", 50, eRadio); // No Mag
	sendTimedCmd("9BA10000002C", 150, eRadio); // No Mag
	sendTimedCmd("9F00CC54", 250, eRadio); // ACK/WAIT
	sendTimedCmd("9BB10010002C", 350, eRadio); // cChanging2
	sendTimedCmd("9BB100100019", 450, eRadio); // cChanging2
	sendTimedCmd("9F002959", 550, eRadio); // ACK/WAIT
	sendTimedCmd("9BC100100010", 650, eRadio); // cChanging3
	sendTimedCmd("9F00296C", 750, eRadio); // ACK/WAIT
	sendTimedCmd("9B810010001C", 850, eRadio); // cChanging4

	sendTimedCmd("9F005561", 950, eRadio); // ACK/WAIT
	sendTimedCmd("9B910000001C", 1050, eRadio); // Changing
	sendTimedCmd("9F005561", 1150, eRadio); // ACK/WAIT
	sendTimedCmd("9910001000000012", 1250, eRadio); // PReparing
	sendTimedCmd("9910001000000012", 1350, eRadio); // no mag
	sendTimedCmd("9F005561", 1450, eRadio); // ACK/WAIT
	sendTimedCmd("9C1012074497D", 1550, eRadio); // cd status
	sendTimedCmd("9F005561", 1650, eRadio); // ACK/WAIT
	sendTimedCmd("9D000000005", 1750, eRadio); // ACK/WAIT
	sendTimedCmd("9950101000000015", 1850, eRadio); // ACK/WAIT
	sendTimedCmd("9F005561", 1950, eRadio); // ACK/WAIT
	if(emuEnabled) {
		cdcEmuPcktEnable();
	}
}


void rLoadDisk(int disk) { //disk 4
	setEmulatorBusy(1300);
	cdcEmuPcktDisable();
	//changeInterruptTimed(eRadio, false, 0);
	player_sec=0;
	curDisk=disk;
	if(emuEnabled) { //if emu enabled, let the user know we are in a "special" mode.
 		curDisk=6+disk;
	}
 	curTrack=1;
	mbus_data_t tmpPacket;
	char localmbus_outbuffer[MBUS_BUFFER];
	tmpPacket.track=INT2BCD(0);
	tmpPacket.index=INT2BCD(1);
	tmpPacket.minutes = INT2BCD(0);
	tmpPacket.seconds = INT2BCD(0);
	tmpPacket.flags=0x000;
	tmpPacket.disk=INT2BCD(disk);

	sendTimedCmd("9F005561", 300, eRadio); // ACK/WAIT

	
	tmpPacket.cmd=cChanging4;
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	sendTimedCmd(localmbus_outbuffer, 400, eRadio);

	tmpPacket.disk=INT2BCD(6+disk);
	tmpPacket.cmd=cChanging4;
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	sendTimedCmd(localmbus_outbuffer, 500, eRadio);

	tmpPacket.cmd=cChanging;
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	sendTimedCmd(localmbus_outbuffer, 600, eRadio);

	sendTimedCmd("9F005561", 700, eRadio); // ACK/WAIT

	tmpPacket.cmd=cPreparing;
	tmpPacket.track=INT2BCD(0);
	tmpPacket.index=INT2BCD(1);
	tmpPacket.minutes = INT2BCD(0);
	tmpPacket.seconds = INT2BCD(0);
	tmpPacket.flags = 0x001;
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	sendTimedCmd(localmbus_outbuffer, 800, eRadio);

	sendTimedCmd("9F005561", 900, eRadio); // ACK/WAIT

	// send disk status (tracks time etc)
	tmpPacket.flags &= ~0xCA0;
	tmpPacket.flags |= 0x007;
	tmpPacket.cmd=cStatus;
	tmpPacket.track=INT2BCD(98); //indicate max tracks for special mode
	tmpPacket.minutes = INT2BCD(74);
	tmpPacket.seconds = INT2BCD(55);
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	sendTimedCmd(localmbus_outbuffer, 1000, eRadio);
	//sendTimedCmd("9Cd012074497D", 600, eRadio); // cd status
	
	//sendTimedCmd("9F005561", 700, eRadio); // ACK/WAIT

	sendTimedCmd("9D000000005", 1100, eRadio); // ACK/WAIT
	sendTimedCmd("9950101000000015", 1200, eRadio); // spinup
	sendTimedCmd("9F005561", 1300, eRadio); // ACK/WAIT
	if(emuEnabled) {
		io_switchInput(disk);
		cdcEmuPcktEnable(); //only resume sending packets if emu is enabled.
	}
}


void cdcChangeTrack(int track) {
	mbus_data_t tmpPacket;
	char localmbus_outbuffer[MBUS_BUFFER];
	tmpPacket.cmd=rSkip;
	tmpPacket.track=INT2BCD(track);
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	sendTimedCmd(localmbus_outbuffer, 50, eCD);
	sendTimedCmd(localmbus_outbuffer, 150, eCD);
}

unsigned int cdcChangeTrackDown_lastUpdate=0;
void cdcChangeTrackDown() {
	if(cdcChangeTrackDown_lastUpdate+1000 > xTaskGetTickCount()) { // Timer for skip track or skip to begin of track as normal cd players do
		curTrack-=1;
	}
	cdcChangeTrack(curTrack);
	cdcChangeTrackDown_lastUpdate=xTaskGetTickCount();
}

void cdcChangeTrackUp() {
	curTrack+=1;
	cdcChangeTrack(curTrack);
}

void cdcChangePlay() {
	mbus_data_t tmpPacket;
	char localmbus_outbuffer[MBUS_BUFFER];
	tmpPacket.cmd=rPlay;
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	sendTimedCmd(localmbus_outbuffer, 50, eCD);
}

void cdcChangePwrDn() {
	mbus_data_t tmpPacket;
	char localmbus_outbuffer[MBUS_BUFFER];
	tmpPacket.cmd=rPowerdown;
	tmpPacket.track=INT2BCD(curTrack);
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	sendTimedCmd(localmbus_outbuffer, 50, eCD);
}

void cdcChangeLoadDisk(int disk) {
	mbus_data_t tmpPacket;
	char localmbus_outbuffer[MBUS_BUFFER];
	tmpPacket.cmd=rSelectDisk;
	tmpPacket.disk=INT2BCD(disk);
	mbus_encode(&tmpPacket, localmbus_outbuffer);
	sendTimedCmd(localmbus_outbuffer, 50, eCD);
}


void mbusEmu_debug(bool newState) {
  emuDebug=true;
}


int phoneModePrev=0;
void emuPhoneMode(bool state) {
	if(state) {
		phoneModePrev=curDisk;
		if(!emuEnabled) { //if not in emu mode, powerdown CDC and spoof track
			disableInterrupt(eCD);
			cdcEmuPcktEnable();
			cdcChangePwrDn();
		} else if(curDisk==8) { //indash cd pause we need to probe the intcd to see if we can resume playback
		}
		//phoneModePrevTrack=curTrack;
		//curTrack=99;
	} else {
		if(!emuEnabled) {	//if not in emu mode, resume CDC
			//curTrack=phoneModePrevTrack;
			changeInterruptTimed(eCD, true, 600);
			cdcChangePlay();
			cdcEmuPcktDisable();
		}		
	}
}

void emuIntCDMode(bool state) {
	if(state) { // switch to intcd
		if(!emuEnabled) { //if emu is disabled switch to emu mode and change disk to 8 (INTCD)
			emuEnabled=true;
			setPassthrough(false);
			activateEmulationMode(2); // (6+2=8)
		} else { //already in emu mode, just switch disk to 8 (intcd)
			rLoadDisk(2); // (6+2=8)
		}
	}
}

void emuBTMode(bool state) {
	if(state) { // switch to intcd
		if(!emuEnabled) { //if emu is disabled switch to emu mode and change disk to 7 (BT)
			emuEnabled=true;
			setPassthrough(false);
			activateEmulationMode(1); // (6+2=7)
		} else { //already in emu mode, just switch disk to 7 (BT)
			rLoadDisk(1); // (6+2=7)
		}
	}
}