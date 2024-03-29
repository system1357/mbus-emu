#ifndef __MBUSEMU_H
#define __MBUSEMU_H
#include "include/mbus.h"

void emulatorMode(const mbus_data_t *inpacket);
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

#endif
