#include <Arduino.h>
#include "settings.h"

#include "AudioPlayer.h"
#include "Cmd.h"
#include "Common.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Queues.h"
#include "Rfid.h"
#include "System.h"
#include "Web.h"

#include "driver/rfid/rfid.hpp"

namespace rfid
{

driver::RfidDriver rfidDriver;

unsigned long Rfid_LastRfidCheckTimestamp = 0;
char gCurrentRfidTagId[cardIdStringSize] = ""; // No crap here as otherwise it could be shown in GUI
#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
	char gOldRfidTagId[cardIdStringSize] = "X";     // Init with crap
#endif

void init() {
	rfidDriver.init();
}

// Tries to lookup RFID-tag-string in NVS and extracts parameter from it if found
void preferenceLookupHandler() {
	// wait for a card change event
	bool cardChangeEvent = rfidDriver.waitForCardEvent(0);
	if(cardChangeEvent) {
		// we got the semaphore
		System_UpdateActivityTimer();

		bool cardPresent = rfidDriver.isCardPresent();
		if(cardPresent) {
			char _file[255];
			uint32_t _lastPlayPos = 0;
			uint16_t _trackLastPlayed = 0;
			uint32_t _playMode = 1;

			// card was put on reader
			const String rfidTagId = rfidDriver.getCardId();
			Log_Printf(LOGLEVEL_INFO, rfidTagReceived, rfidTagId.c_str());
			Web_SendWebsocketData(0, 10); // Push new rfidTagId to all websocket-clients

			String nvsString = gPrefsRfid.getString(rfidTagId.c_str(), "");
			if(nvsString.isEmpty()) {
				// we did not find the RFID Tag in the settings file
				Log_Println(rfidTagUnknownInNvs, LOGLEVEL_ERROR);
				System_IndicateError();
				#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
					strncpy(gOldRfidTagId, gCurrentRfidTagId, cardIdStringSize-1);      // Even if not found in NVS: accept it as card last applied
				#endif
				// allow to escape from bluetooth mode with an unknown card, switch back to normal mode
				System_SetOperationMode(OPMODE_NORMAL);
				return;
			}

			char *token;
			uint8_t i = 1;
			token = strtok(nvsString.begin(), stringDelimiter);
			while (token != NULL) { // Try to extract data from string after lookup
				if (i == 1) {
					size_t len = strnlen(token, sizeof(_file) - 1);
					memcpy(_file, token, len);
					_file[len] = '\0';
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
				Log_Println(errorOccuredNvs, LOGLEVEL_ERROR);
				System_IndicateError();
			} else {
				// Only pass file to queue if strtok revealed 3 items
				if (_playMode >= 100) {
					// Modification-cards can change some settings (e.g. introducing track-looping or sleep after track/playlist).
					Cmd_Action(_playMode);
				} else {
					#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
						if (strncmp(gCurrentRfidTagId, gOldRfidTagId, 12) == 0) {
							Log_Printf(LOGLEVEL_ERROR, dontAccepctSameRfid, gCurrentRfidTagId);
							//System_IndicateError(); // Enable to have shown error @neopixel every time
							return;
						} else {
							strncpy(gOldRfidTagId, gCurrentRfidTagId, 12);
						}
					#endif
					#ifdef MQTT_ENABLE
						publishMqtt(topicRfidState, gCurrentRfidTagId, false);
					#endif

					#ifdef BLUETOOTH_ENABLE
						// if music rfid was read, go back to normal mode
						if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
							System_SetOperationMode(OPMODE_NORMAL);
						}
					#endif

					AudioPlayer_TrackQueueDispatcher(_file, _lastPlayPos, _playMode, _trackLastPlayed);
				}
			}
		} else {
			// card was removed
		}
	}

}

void taskPause() {
	rfidDriver.suspend(true);
}

void taskResume() {
	rfidDriver.suspend(false);
}

void wakeupCheck() {
	rfidDriver.wakeupCheck();
}

} // namespace rfid
