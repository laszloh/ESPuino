#pragma once

constexpr uint8_t cardIdSize = 4u;
constexpr uint8_t cardIdStringSize = (cardIdSize * 3u) + 1u;

extern char gCurrentRfidTagId[cardIdStringSize];

#ifdef DONT_ACCEPT_SAME_RFID_TWICE
void Rfid_ResetOldRfid(void);
#endif

void Rfid_Init(void);
void Rfid_Cyclic(void);
void Rfid_Exit(void);
void Rfid_TaskPause(void);
void Rfid_TaskResume(void);
void Rfid_WakeupCheck(void);
void Rfid_PreferenceLookupHandler(void);
