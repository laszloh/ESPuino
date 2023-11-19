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
static MFRC522DriverI2C driver(MFRC522_ADDR, rfidI2C);
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
	MFRC522::PCD_Version firmwareVersion = mfrc522.PCD_GetVersion();
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
		0 /* Core where the task should run */
	);
}

void Rfid_Task(void *parameter) {
	uint32_t lastTimeCardDetect = 0;
	CardIdType lastCardId;
	bool cardAppliedLastRun = false;

	while (1) {
		bool cardAppliedCurrentRun = false;

		if constexpr (RFID_SCAN_INTERVAL / 2 >= 20) {
			vTaskDelay(portTICK_PERIOD_MS * (RFID_SCAN_INTERVAL / 2));
		} else {
			vTaskDelay(portTICK_PERIOD_MS * 20);
		}

		uint8_t bufferATQA[8];
		uint8_t bufferSize = sizeof(bufferATQA);

		// wake up one card on the reader
		MFRC522::StatusCode result = mfrc522.PICC_WakeupA(bufferATQA, &bufferSize);
		if (result == MFRC522::StatusCode::STATUS_OK) {
			// we found or woke up a card, read id
			mfrc522.PICC_Select(&mfrc522.uid, 0);
			cardAppliedCurrentRun = (result == MFRC522::StatusCode::STATUS_OK);

			// Bring card into HALT mode
			mfrc522.PICC_HaltA();
			mfrc522.PCD_StopCrypto1();
		}

		if (cardAppliedCurrentRun) {
			lastTimeCardDetect = millis();
			cardAppliedLastRun = true;

			CardIdType cardId;
			cardId.assign(mfrc522.uid.uidByte);

			if (cardId == lastCardId) {
				// this is the same card
				continue;
			}

			// different card id read
			Message msg;
			msg.event = Message::Event::CardApplied;
			msg.cardId = cardId;

			Log_Printf(LOGLEVEL_NOTICE, rfidTagDetected, cardId.toHexString().c_str());
			lastCardId = cardId;

			Rfid_SignalEvent(msg);
		} else {
			if (!lastTimeCardDetect || (millis() - lastTimeCardDetect) > cardDetectTimeout) {
				// card was removed for sure
				lastTimeCardDetect = 0;
				if (cardAppliedLastRun) {
					// send the card removed event
					Message msg;
					msg.event = Message::Event::CardRemoved;
					msg.cardId = lastCardId;

					Rfid_SignalEvent(msg);
				}
				cardAppliedLastRun = false;
				lastCardId = {};
			}
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
