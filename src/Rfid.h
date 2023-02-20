#pragma once

constexpr uint8_t cardIdSize = 4u;
constexpr uint8_t cardIdStringSize = (cardIdSize * 3u) + 1u;

extern char gCurrentRfidTagId[cardIdStringSize];

#ifndef PAUSE_WHEN_RFID_REMOVED
	#ifdef DONT_ACCEPT_SAME_RFID_TWICE      // ignore feature silently if PAUSE_WHEN_RFID_REMOVED is active
		#define DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
	#endif
#endif

#if defined(DONT_ACCEPT_SAME_RFID_TWICE_ENABLE) || defined(PAUSE_WHEN_RFID_REMOVED)
	extern char gOldRfidTagId[cardIdStringSize];
#endif

enum class RfidEvent : uint8_t {
	NoCard = 0,
	CardRemoved,
	CardApplied,
	CardPresent
};

struct RfidMessage {
	RfidEvent event;
	uint8_t cardId[cardIdSize];
};

void Rfid_Init(void);
void Rfid_Cyclic(void);
void Rfid_Exit(void);
void Rfid_WakeupCheck(void);
void Rfid_PreferenceLookupHandler(void);
