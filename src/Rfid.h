#pragma once

// constexpr uint8_t cardIdSize = 4u;
// constexpr uint8_t cardIdStringSize = (cardIdSize * 3u) + 1u;

// extern char gCurrentRfidTagId[cardIdStringSize];

#ifndef PAUSE_WHEN_RFID_REMOVED
	#ifdef DONT_ACCEPT_SAME_RFID_TWICE      // ignore feature silently if PAUSE_WHEN_RFID_REMOVED is active
		#define DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
	#endif
#endif

#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
	extern char gOldRfidTagId[cardIdStringSize];
#endif

template <typename T> void Rfid_Init(T& base) { base.init(); }
template <typename T> void Rfid_Cyclic(T& base) { base.cyclic(); }
template <typename T> void Rfid_Exit(T& base) { base.exit(); }
template <typename T> void Rfid_WakeupCheck(T& base) { base.wakeupCheck(); }
template <typename T> void Rfid_PreferenceLookupHandler(T& base) { base.preferenceLookupHandler(); }

#include "crtp.h"
#include <stdint.h>

#define RFID_READER_ENABLED 1

template <typename Derived>
class Rfid : crtp<Derived, Rfid> {
public:
	void init() { this->underlying().init(); }

	void cyclic() {	this->underlying().cyclic(); }

	void exit() { this->underlying().exit(); }

	void wakeupCheck() { this->underlying().wakeupCheck(); }

	void preferenceLookupHandler() { 
#ifdef RFID_READER_ENABLED
		BaseType_t rfidStatus;
		char rfidTagId[cardIdStringSize];
		char _file[255];
		uint32_t _lastPlayPos = 0;
		uint16_t _trackLastPlayed = 0;
		uint32_t _playMode = 1;

		rfidStatus = xQueueReceive(gRfidCardQueue, &rfidTagId, 0);
		if (rfidStatus != pdPASS)
			return;

		System_UpdateActivityTimer();
		strncpy(currentRfidTagId, rfidTagId, cardIdStringSize-1);
		Web_SendWebsocketData(0, 10); // Push new rfidTagId to all websocket-clients
		snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(rfidTagReceived), currentRfidTagId);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);

		String s = gPrefsRfid.getString(currentRfidTagId); // Try to lookup rfidId in NVS
		if (!s) {
			Log_Println((char *) FPSTR(rfidTagUnknownInNvs), LOGLEVEL_ERROR);
			System_IndicateError();
#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
				strncpy(oldRfidTagId, currentRfidTagId, cardIdStringSize-1);      // Even if not found in NVS: accept it as card last applied
#endif
			return;
		}

		char *token;
		uint8_t i = 1;
		token = strtok(s.begin(), stringDelimiter);
		while (token != NULL) { // Try to extract data from string after lookup
			if (i == 1) {
				strncpy(_file, token, sizeof(_file)-1);
			} else if (i == 2) {
				_lastPlayPos = strtoul(token, NULL, 10);
			} else if (i == 3) {
				_playMode = strtoul(token, NULL, 10);
			} else if (i == 4) {
				_trackLastPlayed = strtoul(token, NULL, 10);
			}
			i++;
			token = strtok(NULL, stringDelimiter);
		}

		if (i != 5) {
			Log_Println((char *) FPSTR(errorOccuredNvs), LOGLEVEL_ERROR);
			System_IndicateError();
			return;
		}
		// Only pass file to queue if strtok revealed 4 items
		if (_playMode >= 100) {
			// Modification-cards can change some settings (e.g. introducing track-looping or sleep after track/playlist).
			Cmd_Action(_playMode);
		} else {
#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
				if (strncmp(currentRfidTagId, oldRfidTagId, cardIdStringSize-1) == 0) {
					snprintf(Log_Buffer, Log_BufferLength, "%s (%s)", (char *) FPSTR(dontAccepctSameRfid), currentRfidTagId);
					Log_Println(Log_Buffer, LOGLEVEL_ERROR);
					//System_IndicateError(); // Enable to have shown error @neopixel every time
					return;
				}
				strncpy(oldRfidTagId, currentRfidTagId, cardIdStringSize-1);
#endif
#ifdef MQTT_ENABLE
				publishMqtt((char *) FPSTR(topicRfidState), currentRfidTagId, false);
#endif

#ifdef BLUETOOTH_ENABLE
				// if music rfid was read, go back to normal mode
				if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
					System_SetOperationMode(OPMODE_NORMAL);
				}
#endif

			AudioPlayer_TrackQueueDispatcher(_file, _lastPlayPos, _playMode, _trackLastPlayed);
		}
#endif
	 }

	const char *getCurrentRfidTagId() const {
		return currentRfidTagId;
	}

private:
	static constexpr uint8_t cardIdSize = 4u;
	static constexpr uint8_t cardIdStringSize = (cardIdSize * 3u) + 1u;

	char currentRfidTagId[cardIdStringSize];

#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
	char oldRfidTagId[cardIdStringSize];
#endif

	Rfid(){};
	friend Derived;
};
