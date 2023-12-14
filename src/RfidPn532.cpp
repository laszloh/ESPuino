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

#include <Adafruit_PN532.h>

#if defined(RFID_READER_TYPE_PN532)

TaskHandle_t rfidTaskHandle;
static void Rfid_Task(void *parameter);

	#if defined(INTERFACE_I2C)
Adafruit_PN532 pn532 {Adafruit_PN532(RFID_IRQ, 99, i2cBusTwo)}; // Create PN532 instance.
	#elif defined(INTERFACE_SPI)
Adafruit_PN532 pn532 {Adafruit_PN532(RFID_CS, &SPI)};
	#endif

void Rfid_Driver_Init(void) {
	#ifdef INTERFACE_SPI
	SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
	SPI.setFrequency(100000);
	#endif
	pn532.begin();
	pn532.wakeup();

	const uint32_t version = pn532.getFirmwareVersion();
	if (!version) {
		Log_Println("Did not find NFC card reader!", LOGLEVEL_ERROR);
		return;
	}
	Log_Printf(LOGLEVEL_NOTICE, "Found PN5%X FW: %d.%d", (version >> 24) & 0xFF, (version >> 16) & 0xFF, (version >> 8) & 0xFF);
	pn532.setPassiveActivationRetries(0x05);
	pn532.SAMConfig();

	Log_Println(rfidScannerReady, LOGLEVEL_DEBUG);

	xTaskCreatePinnedToCore(
		Rfid_Task,
		"rfid",
		2048,
		NULL,
		2 | portPRIVILEGE_BIT,
		&rfidTaskHandle,
		0);
}

void Rfid_Task(void *parameter) {
	uint32_t lastTimeCardDetect = 0;
	CardIdType lastCardId;
	bool cardAppliedLastRun = false;

	while (1) {
		uint8_t uid[10];
		uint8_t uidLen;

		if constexpr (RFID_SCAN_INTERVAL / 2 >= 20) {
			vTaskDelay(portTICK_PERIOD_MS * (RFID_SCAN_INTERVAL / 2));
		} else {
			vTaskDelay(portTICK_PERIOD_MS * 20);
		}

		const uint32_t version = pn532.getFirmwareVersion();
		if (!version) {
			Log_Println("Lost contact to the NFC card reader!", LOGLEVEL_ERROR);
			pn532.begin();
			pn532.wakeup();
		}

		bool cardAppliedCurrentRun = pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen);
		if (cardAppliedCurrentRun) {
			lastTimeCardDetect = millis();
			cardAppliedLastRun = true;
			CardIdType cardId;
			cardId.assign(uid);

			if (cardId == lastCardId) {
				// this is the same card
				continue;
			}
			lastCardId = cardId;

			// different card id read
			Message msg;
			msg.event = Message::Event::CardApplied;
			msg.cardId = cardId;

			Log_Printf(LOGLEVEL_NOTICE, rfidTagDetected, cardId.toHexString().c_str());
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
	uint8_t buffer[3];
	buffer[0] = PN532_COMMAND_POWERDOWN;
	buffer[1] = 0x20; //(0x20, for SPI) (0x28 for SPI and RF detectionThe wakeup source(s) you want too use
	buffer[2] = 0x01; // To eneable the IRQ, 0x00 if you dont want too use the IRQ

	pn532.sendCommandCheckAck(buffer, 3);
}

void Rfid_WakeupCheck(void) {
}

#endif
