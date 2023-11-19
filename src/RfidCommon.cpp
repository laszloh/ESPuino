#include <Arduino.h>
#include "settings.h"

#include "AudioPlayer.h"
#include "Cmd.h"
#include "Common.h"
#include "HallEffectSensor.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Queues.h"
#include "Rfid.h"
#include "System.h"
#include "Web.h"

#include <mutex>

#if defined(PAUSE_WHEN_RFID_REMOVED) && defined(DONT_ACCEPT_SAME_RFID_TWICE)
	#error "PAUSE_WHEN_RFID_REMOVED and DONT_ACCEPT_SAME_RFID_TWICE can not be activated at the same time"
#endif

unsigned long Rfid_LastRfidCheckTimestamp = 0;
char gCurrentRfidTagId[cardIdStringSize] = ""; // No crap here as otherwise it could be shown in GUI

#if defined(RFID_READER_ENABLED)

extern TaskHandle_t rfidTaskHandle;

// variables for the event system, do not use directly, they will be written from different tasks!
static SemaphoreHandle_t rfidEvent = xSemaphoreCreateBinary();
static std::mutex messageGuard;
static Message message;

// internal variables
static CardIdType currentRfidTagId;
static CardIdType oldRfidTagId __attribute__((unused));

void Rfid_Driver_Init();
void Rfid_CardReceivedEvent(const Message &msg);
void Rfid_CardRemovedEvent(const Message &msg);

void Rfid_Init() {
	// call the driver init
	Rfid_Driver_Init();
}

// Tries to lookup RFID-tag-string in NVS and extracts parameter from it if found
void Rfid_Cyclic() {
	BaseType_t ret = xSemaphoreTake(rfidEvent, 0);
	if (ret) {
		System_UpdateActivityTimer();

		// get the new message
		Message msg;
		{ // Don't remove because of the lifetime of the mutex
			std::lock_guard<std::mutex> guard(messageGuard);
			msg = message;
		}

		switch (msg.event) {
			case Message::Event::CardApplied:
				Rfid_CardReceivedEvent(msg);
				break;

			case Message::Event::CardRemoved:
				Rfid_CardRemovedEvent(msg);
				break;

			default:
				break;
		}
	}
}

void Rfid_CardReceivedEvent(const Message &msg) {
	String _file;
	uint32_t _lastPlayPos = 0;
	uint16_t _trackLastPlayed = 0;
	uint32_t _playMode = 1;

	currentRfidTagId = msg.cardId;

	#ifdef HALLEFFECT_SENSOR_ENABLE
	currentRfidTagId[currentRfidTagId.size() - 1] += gHallEffectSensor.waitForState(HallEffectWaitMS);
	#endif

	const String newCardId = currentRfidTagId.toDezimalString();

	Log_Printf(LOGLEVEL_INFO, rfidTagReceived, newCardId.c_str());
	Web_SendWebsocketData(0, 10); // Push new rfidTagId to all websocket-clients
	if (!gPrefsRfid.isKey(newCardId.c_str())) {
		// we do not know this card id
		Log_Println(rfidTagUnknownInNvs, LOGLEVEL_ERROR);
		System_IndicateError();
	#ifdef DONT_ACCEPT_SAME_RFID_TWICE
		oldRfidTagId = currentRfidTagId;
	#endif
		// allow to escape from bluetooth mode with an unknown card, switch back to normal mode
		System_SetOperationMode(OPMODE_NORMAL);
		return;
	}

	// we know this card, so get all the infos
	String value = gPrefsRfid.getString(gCurrentRfidTagId);
	char *token;
	uint8_t i = 1;
	token = strtok(value.begin(), stringDelimiter);
	while (token != NULL) { // Try to extract data from string after lookup
		if (i == 1) {
			_file = token;
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
		return;
	}

	// Only pass file to queue if strtok revealed 3 items
	if (_playMode >= 100) {
		// Modification-cards can change some settings (e.g. introducing track-looping or sleep after track/playlist).
		Cmd_Action(_playMode);
	} else {
	#ifdef DONT_ACCEPT_SAME_RFID_TWICE
		if (currentRfidTagId == oldRfidTagId) {
			Log_Printf(LOGLEVEL_ERROR, dontAccepctSameRfid, gCurrentRfidTagId);
			// System_IndicateError(); // Enable to have shown error @neopixel every time
			return;
		} else {
			oldRfidTagId = currentRfidTagId;
		}
	#endif
	#ifdef MQTT_ENABLE
		publishMqtt(topicRfidState, newCardId.c_str(), false);
	#endif

	#ifdef BLUETOOTH_ENABLE
		// if music rfid was read, go back to normal mode
		if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
			System_SetOperationMode(OPMODE_NORMAL);
		}
	#endif
		AudioPlayer_TrackQueueDispatcher(_file.c_str(), _lastPlayPos, _playMode, _trackLastPlayed);
	}
}

void Rfid_CardRemovedEvent(const Message &msg) {
}

void Rfid_ResetOldRfid() {
	#ifdef DONT_ACCEPT_SAME_RFID_TWICE
	strncpy(gOldRfidTagId, "X", cardIdStringSize - 1);
	#endif
}

void Rfid_TaskPause() {
	vTaskSuspend(rfidTaskHandle);
}

void Rfid_TaskResume() {
	vTaskResume(rfidTaskHandle);
}

void Rfid_SignalEvent(const Message &msg) {
	{ // Don't remove because of the lifetime of the mutex
		std::lock_guard<std::mutex> guard(messageGuard);
		message = msg;
	}

	// signal the event
	xSemaphoreGive(rfidEvent);
}

#else

// no RFID_READER_ENABLED enabled, implement empty fuctions

void Rfid_Init() {
}

void Rfid_Cyclic() {
}

void Rfid_Exit() {
}

void Rfid_TaskPause() {
}

void Rfid_TaskResume() {
}

void Rfid_WakeupCheck() {
}

void Rfid_SignalEvent(const Message &msg) {
}

#endif
