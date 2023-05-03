#pragma once

namespace rfid
{

void init();
void exit();
void taskPause();
void taskResume();
void wakeupCheck();
void preferenceLookupHandler();

} // namespace rfid


constexpr uint8_t cardIdStringSize = (4 * 3u) + 1u;

extern char gCurrentRfidTagId[cardIdStringSize];

#ifndef PAUSE_WHEN_RFID_REMOVED
	#ifdef DONT_ACCEPT_SAME_RFID_TWICE // ignore feature silently if PAUSE_WHEN_RFID_REMOVED is active
		#define DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
	#endif
#endif

#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
void Rfid_ResetOldRfid(void);
#endif

