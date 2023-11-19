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

#if defined(PAUSE_WHEN_RFID_REMOVED) && defined(DONT_ACCEPT_SAME_RFID_TWICE)
	#error "PAUSE_WHEN_RFID_REMOVED and DONT_ACCEPT_SAME_RFID_TWICE_ENABLE can not be enabled at the same time"
#endif

namespace rfid {

using namespace driver;

RfidDriver rfidDriver;
CardIdType oldRfidCard;

void executeCardAppliedEvent(const Message &msg);
void executeCardRemoveEvent(const Message &msg);

void init() {
	rfidDriver.init();
}

void exit() {
	rfidDriver.exit();
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

// Tries to lookup RFID-tag-string in NVS and extracts parameter from it if found
void cyclic() {
	// wait for a card change event
	bool cardChangeEvent = rfidDriver.waitForCardEvent(0);
	if (cardChangeEvent) {
		Message msg = rfidDriver.getLastEvent();

		// we got the semaphore
		System_UpdateActivityTimer();

		switch (msg.event) {
			case Message::Event::CardApplied:
				log_n("applied: %s", msg.toDezimalString().c_str());
				executeCardAppliedEvent(msg);
				break;

			case Message::Event::CardRemoved:
				executeCardRemoveEvent(msg);
				log_n("removed");
				break;

			case Message::Event::NoCard:
			case Message::Event::CardPresent:
				break;
		}
	}
}

void resetOldRfid() {
#ifdef DONT_ACCEPT_SAME_RFID_TWICE
		oldRfidCard = {};
#endif
}

void executeCardAppliedEvent(const Message &msg) {
	char _file[255];
	uint32_t _lastPlayPos = 0;
	uint16_t _trackLastPlayed = 0;
	uint32_t _playMode = 1;
	// card was put on reader
	const String rfidTagId = msg.toDezimalString();
	Log_Printf(LOGLEVEL_INFO, rfidTagReceived, rfidTagId.c_str());
	Web_SendWebsocketData(0, 10); // Push new rfidTagId to all websocket-clients

	String nvsString = gPrefsRfid.getString(rfidTagId.c_str(), "");
	if (nvsString.isEmpty()) {
		// we did not find the RFID Tag in the settings file
		Log_Println(rfidTagUnknownInNvs, LOGLEVEL_ERROR);
		System_IndicateError();
#ifdef DONT_ACCEPT_SAME_RFID_TWICE
		oldRfidCard = msg.cardId;
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
#ifdef DONT_ACCEPT_SAME_RFID_TWICE
			if (oldRfidCard == msg.cardId) {
				Log_Printf(LOGLEVEL_ERROR, dontAccepctSameRfid, msg.toDezimalString().c_str());
				// System_IndicateError(); // Enable to have shown error @neopixel every time
				return;
			}
			oldRfidCard = msg.cardId;
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

#ifdef PAUSE_WHEN_RFID_REMOVED
			if (oldRfidCard == msg.cardId) {
				// resume playback
				AudioPlayer_TrackControlToQueueSender(PLAY);
				return;
			}
#endif

			AudioPlayer_TrackQueueDispatcher(_file, _lastPlayPos, _playMode, _trackLastPlayed);
		}
	}
}

void executeCardRemoveEvent(const Message &msg) {
#ifdef PAUSE_WHEN_RFID_REMOVED
	// save the olf rfid card
	oldRfidCard = msg.cardId;

	Log_Println(rfidTagRemoved, LOGLEVEL_NOTICE);
	if (System_GetOperationMode() != OPMODE_BLUETOOTH_SINK && !gPlayProperties.playlistFinished) {
		AudioPlayer_TrackControlToQueueSender(PAUSE);
		Log_Println(rfidTagReapplied, LOGLEVEL_NOTICE);
	}

#endif
}

} // namespace rfid
