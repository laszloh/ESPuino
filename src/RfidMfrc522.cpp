#include <Arduino.h>
#include "settings.h"

#include "AudioPlayer.h"
#include "HallEffectSensor.h"
#include "Log.h"
#include "MemX.h"
#include "Queues.h"
#include "Rfid.h"
#include "System.h"
#include "Wire.h"

#include <MFRC522DriverI2C.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522v2.h>
#include <esp_task_wdt.h>

#if defined(RFID_READER_TYPE_MFRC522)

extern unsigned long Rfid_LastRfidCheckTimestamp;
TaskHandle_t rfidTaskHandle;
static void Rfid_Task(void *parameter);

	#if defined(INTERFACE_I2C)
extern TwoWire i2cBusTwo;
static MFRC522DriverI2C driver(MFRC522_ADDR, &i2cBusTwo);
	#elif defined(INTERFACE_SPI)
static MFRC522DriverPinSimple ss_pin(RFID_CS);
static MFRC522DriverSPI driver(ss_pin);
	#endif

static MFRC522 mfrc522(driver);

void Rfid_Driver_Init(void) {
	#ifdef INTERFACE_SPI
	SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
	SPI.setFrequency(1000000);
	#endif

	// Init RC522 Card-Reader
	mfrc522.PCD_Init();
	delay(10);
	// Get the MFRC522 firmware version, should be 0x91 or 0x92
	byte firmwareVersion = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
	Log_Printf(LOGLEVEL_DEBUG, "RC522 firmware version=%#lx", firmwareVersion);

	mfrc522.PCD_SetAntennaGain(rfidGain);
	delay(50);
	Log_Println(rfidScannerReady, LOGLEVEL_DEBUG);

	xTaskCreatePinnedToCore(
		Rfid_Task, /* Function to implement the task */
		"rfid", /* Name of the task */
		2048, /* Stack size in words */
		NULL, /* Task input parameter */
		2 | portPRIVILEGE_BIT, /* Priority of the task */
		&rfidTaskHandle, /* Task handle. */
		1 /* Core where the task should run */
	);
}

void Rfid_Task(void *parameter) {
	#ifdef PAUSE_WHEN_RFID_REMOVED
	uint8_t control = 0x00;
	#endif

	for (;;) {
		if (RFID_SCAN_INTERVAL / 2 >= 20) {
			vTaskDelay(portTICK_PERIOD_MS * (RFID_SCAN_INTERVAL / 2));
		} else {
			vTaskDelay(portTICK_PERIOD_MS * 20);
		}
		byte cardId[cardIdSize];
		String cardIdString;
	#ifdef PAUSE_WHEN_RFID_REMOVED
		byte lastValidcardId[cardIdSize];
		bool sameCardReapplied = false;
	#endif
		if ((millis() - Rfid_LastRfidCheckTimestamp) >= RFID_SCAN_INTERVAL) {
			// Log_Printf(LOGLEVEL_DEBUG, "%u", uxTaskGetStackHighWaterMark(NULL));

			Rfid_LastRfidCheckTimestamp = millis();
			// Reset the loop if no new card is present on the sensor/reader. This saves the entire process when idle.

			if (!mfrc522.PICC_IsNewCardPresent()) {
				continue;
			}

			// Select one of the cards
			if (!mfrc522.PICC_ReadCardSerial()) {
				continue;
			}

	#ifndef PAUSE_WHEN_RFID_REMOVED
			mfrc522.PICC_HaltA();
			mfrc522.PCD_StopCrypto1();
	#endif

			memcpy(cardId, mfrc522.uid.uidByte, cardIdSize);

	#ifdef HALLEFFECT_SENSOR_ENABLE
			cardId[cardIdSize - 1] = cardId[cardIdSize - 1] + gHallEffectSensor.waitForState(HallEffectWaitMS);
	#endif

	#ifdef PAUSE_WHEN_RFID_REMOVED
			if (memcmp((const void *) lastValidcardId, (const void *) cardId, sizeof(cardId)) == 0) {
				sameCardReapplied = true;
			}
	#endif

			String hexString;
			for (uint8_t i = 0u; i < cardIdSize; i++) {
				char str[4];
				snprintf(str, sizeof(str), "%02x%c", cardId[i], (i < cardIdSize - 1u) ? '-' : ' ');
				hexString += str;
			}
			Log_Printf(LOGLEVEL_NOTICE, rfidTagDetected, hexString.c_str());

			for (uint8_t i = 0u; i < cardIdSize; i++) {
				char num[4];
				snprintf(num, sizeof(num), "%03d", cardId[i]);
				cardIdString += num;
			}

	#ifdef PAUSE_WHEN_RFID_REMOVED
		#ifdef ACCEPT_SAME_RFID_AFTER_TRACK_END
			if (!sameCardReapplied || gPlayProperties.trackFinished || gPlayProperties.playlistFinished) { // Don't allow to send card to queue if it's the same card again if track or playlist is unfnished
		#else
			if (!sameCardReapplied) { // Don't allow to send card to queue if it's the same card again...
		#endif
				xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);
			} else {
				// If pause-button was pressed while card was not applied, playback could be active. If so: don't pause when card is reapplied again as the desired functionality would be reversed in this case.
				if (gPlayProperties.pausePlay && System_GetOperationMode() != OPMODE_BLUETOOTH_SINK) {
					AudioPlayer_TrackControlToQueueSender(PAUSEPLAY); // ... play/pause instead (but not for BT)
				}
			}
			memcpy(lastValidcardId, mfrc522.uid.uidByte, cardIdSize);
	#else
			xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0); // If PAUSE_WHEN_RFID_REMOVED isn't active, every card-apply leads to new playlist-generation
	#endif

	#ifdef PAUSE_WHEN_RFID_REMOVED
			// https://github.com/miguelbalboa/rfid/issues/188; voodoo! :-)
			while (true) {
				if (RFID_SCAN_INTERVAL / 2 >= 20) {
					vTaskDelay(portTICK_PERIOD_MS * (RFID_SCAN_INTERVAL / 2));
				} else {
					vTaskDelay(portTICK_PERIOD_MS * 20);
				}
				control = 0;
				for (uint8_t i = 0u; i < 3; i++) {
					if (!mfrc522.PICC_IsNewCardPresent()) {
						if (mfrc522.PICC_ReadCardSerial()) {
							control |= 0x16;
						}
						if (mfrc522.PICC_ReadCardSerial()) {
							control |= 0x16;
						}
						control += 0x1;
					}
					control += 0x4;
				}

				if (control == 13 || control == 14) {
					// card is still there
				} else {
					break;
				}
			}

			Log_Println(rfidTagRemoved, LOGLEVEL_NOTICE);
			if (!gPlayProperties.pausePlay && System_GetOperationMode() != OPMODE_BLUETOOTH_SINK) {
				AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);
				Log_Println(rfidTagReapplied, LOGLEVEL_NOTICE);
			}
			mfrc522.PICC_HaltA();
			mfrc522.PCD_StopCrypto1();
	#endif
		}
	}
}

void Rfid_Exit(void) {
	#ifndef INTERFACE_I2C
	mfrc522.PCD_SoftPowerDown();
	#endif
}

void Rfid_WakeupCheck(void) {
}

#endif
