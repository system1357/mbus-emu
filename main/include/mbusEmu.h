#ifndef __MBUSEMU_H
#define __MBUSEMU_H
#include "include/mbus.h"

// config
#include "include/config.h"

esp_timer_handle_t timer_cdcEmu_handle;


void emulatorMode(const mbus_data_t *inpacket);
void mbusEmuModeChange(bool newState);
void mbusEmuInit();

void cdcEmuPcktDisable();
void cdcEmuPcktEnable();

void cdcChangeTrack(int track);
void cdcChangeTrackDown();
void cdcChangeTrackUp();
void cdcChangePwrDn();
void cdcChangePlay();
void cdcChangeEjectMag();
void cdcChangeInsertMag();
void cdcChangeLoadDisk(int disk);
void rLoadDisk(int disk);

void emuPhoneMode(bool state);
void emuIntCDMode(bool state);
void emuBTMode(bool state);


#endif
