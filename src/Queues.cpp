#include <Arduino.h>
#include "settings.h"

#include "Log.h"
#include "Playlist.h"
#include "Rfid.h"

QueueHandle_t gVolumeQueue;
QueueHandle_t gTrackQueue;
QueueHandle_t gTrackControlQueue;

void Queues_Init(void) {
	// Create queues
	gVolumeQueue = xQueueCreate(1, sizeof(int));
	if (gVolumeQueue == NULL) {
		Log_Println(unableToCreateVolQ, LOGLEVEL_ERROR);
	}

	gTrackControlQueue = xQueueCreate(1, sizeof(uint8_t));
	if (gTrackControlQueue == NULL) {
		Log_Println(unableToCreateMgmtQ, LOGLEVEL_ERROR);
	}

	gTrackQueue = xQueueCreate(1, sizeof(Playlist *));
	if (gTrackQueue == NULL) {
		Log_Println(unableToCreatePlayQ, LOGLEVEL_ERROR);
	}
}
