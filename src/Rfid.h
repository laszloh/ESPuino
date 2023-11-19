#pragma once

#include "RfidEvent.h"

namespace rfid
{

void init();
void cyclic();
void exit();
void taskPause();
void taskResume();
void wakeupCheck();

void resetOldRfid();

} // namespace rfid


constexpr uint8_t cardIdStringSize = (4 * 3u) + 1u;

extern char gCurrentRfidTagId[cardIdStringSize];

// #ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
// void Rfid_ResetOldRfid(void);
// #endif

