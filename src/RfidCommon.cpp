#include <Arduino.h>
#include "settings.h"
#include "Rfid.h"
#include "AudioPlayer.h"
#include "Cmd.h"
#include "Common.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Queues.h"
#include "System.h"
#include "Web.h"

char gCurrentRfidTagId[cardIdStringSize] = "";		// No crap here as otherwise it could be shown in GUI
#if defined(DONT_ACCEPT_SAME_RFID_TWICE_ENABLE) || defined(PAUSE_WHEN_RFID_REMOVED)
	char gOldRfidTagId[cardIdStringSize] = "";     // Init with crap
#endif

static inline void RFID_IdToStr(const uint8_t *buf, char *str) {
	for(size_t i=0;i<cardIdSize;i++) {
		*(str++) = '0' + buf[i] / 100;
		*(str++) = '0' + (buf[i] / 10) % 10;
		*(str++) = '0' + buf[i] % 10;
	}
	*str = '\0';
}

// Tries to lookup RFID-tag-string in NVS and extracts parameter from it if found
void Rfid_PreferenceLookupHandler(void) {
	#if defined (RFID_READER_TYPE_MFRC522_SPI) || defined (RFID_READER_TYPE_MFRC522_I2C) || defined(RFID_READER_TYPE_PN5180) || defined(RFID_READER_TYPE_PN532_I2C) || defined(RFID_READER_TYPE_PN532_SPI)
		BaseType_t rfidStatus;
		RfidMessage msg;
		char _file[255];
		uint32_t _lastPlayPos = 0;
		uint16_t _trackLastPlayed = 0;
		uint32_t _playMode = 1;

		rfidStatus = xQueueReceive(gRfidCardQueue, &msg, 0);
		if (rfidStatus != pdPASS) {
			return;
		}
			
		if(msg.event == RfidEvent::NoCard || msg.event == RfidEvent::CardPresent) {
			// silently ignore non state-change messages
			// current RFID modules do not send them
			return;
		} else if(msg.event == RfidEvent::CardRemoved) {
			// a card was removed from the field
			#ifdef PAUSE_WHEN_RFID_REMOVED
				// pause playback if "our" card was the one removed (but not for BT)
				char removedCardId[cardIdStringSize];
				RFID_IdToStr(msg.cardId, removedCardId);
				if(System_GetOperationMode() != OPMODE_BLUETOOTH_SINK && memcmp(removedCardId, gOldRfidTagId, cardIdSize) == 0) {
					AudioPlayer_TrackControlToQueueSender(PAUSE);
				}
			#endif
			//	memset(gCurrentRfidTagId, 0, sizeof(gCurrentRfidTagId));	// ToDo: Do we "remove" the current rfid in such a case?
			return;
		}

		// else we have a card
		System_UpdateActivityTimer();
		RFID_IdToStr(msg.cardId, gCurrentRfidTagId);
		snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(rfidTagReceived), gCurrentRfidTagId);
		Web_SendWebsocketData(0, 10); // Push new rfidTagId to all websocket-clients
		Log_Println(Log_Buffer, LOGLEVEL_INFO);

		String s = gPrefsRfid.getString(gCurrentRfidTagId, "-1"); // Try to lookup rfidId in NVS
		if (s.compareTo("-1") == 0) {
			Log_Println((char *) FPSTR(rfidTagUnknownInNvs), LOGLEVEL_ERROR);
			System_IndicateError();
			#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
				memcpy(gOldRfidTagId, gCurrentRfidTagId, cardIdStringSize);      // Even if not found in NVS: accept it as card last applied
			#endif
			return;
		}

		char *token;
		uint8_t i = 1;
		token = strtok(s.begin(), stringDelimiter);
		while (token != NULL) { // Try to extract data from string after lookup
			if (i == 1) {
				strncpy(_file, token, sizeof(_file) / sizeof(_file[0]));
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
		} else {
			// Only pass file to queue if strtok revealed 3 items
			if (_playMode >= 100) {
				// Modification-cards can change some settings (e.g. introducing track-looping or sleep after track/playlist).
				Cmd_Action(_playMode);
			} else {
				if(gPlayProperties.playMode != NO_PLAYLIST) {
					// we are currently in playback mode, so do the special stuff
					#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
						if (strncmp(gCurrentRfidTagId, gOldRfidTagId, cardIdStringSize) == 0) {
							snprintf(Log_Buffer, Log_BufferLength, "%s (%s)", (char *) FPSTR(dontAccepctSameRfid), gCurrentRfidTagId);
							Log_Println(Log_Buffer, LOGLEVEL_ERROR);
							//System_IndicateError(); // Enable to have shown error @neopixel every time
							return;
						}
					#endif
					#ifdef PAUSE_WHEN_RFID_REMOVED
						if (strncmp(gCurrentRfidTagId, gOldRfidTagId, cardIdStringSize) == 0) {
							// we saw our "old" card, so start the playback
							if(System_GetOperationMode() != OPMODE_BLUETOOTH_SINK) {
								AudioPlayer_TrackControlToQueueSender(PLAY);
							}
							return;
						}
					#endif
				}
				#ifdef MQTT_ENABLE
					publishMqtt((char *) FPSTR(topicRfidState), gCurrentRfidTagId, false);
				#endif

				#ifdef BLUETOOTH_ENABLE
					// if music rfid was read, go back to normal mode
					if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
						System_SetOperationMode(OPMODE_NORMAL);
					}
				#endif

				memcpy(gOldRfidTagId, gCurrentRfidTagId, cardIdStringSize);
				AudioPlayer_TrackQueueDispatcher(_file, _lastPlayPos, _playMode, _trackLastPlayed);
			}
		}
	#endif
}
