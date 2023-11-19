#pragma once

#include <Arduino.h>
#include "settings.h"

#include "Log.h"
#include "Wire.h"

#include <MFRC522DriverI2C.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522v2.h>

extern TwoWire i2cBusTwo;

namespace rfid::driver {
namespace implementation {

class Mfrc522Driver : public RfidDriverBase<Mfrc522Driver> {
public:
	void init() {
#ifdef RFID_READER_TYPE_MFRC522_SPI
		SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
		SPI.setFrequency(1000000);
#endif
		mfrc522.PCD_Init();
		mfrc522.PCD_SetAntennaGain(rfidGain);
		delay(50);
		Log_Println(rfidScannerReady, LOGLEVEL_DEBUG);

		xTaskCreatePinnedToCore(
			Mfrc522Driver::Task,
			"rfid",
			2048,
			this,
			2 | portPRIVILEGE_BIT,
			&taskHandle,
			ARDUINO_RUNNING_CORE);
	}

	void suspend(bool enable) {
		if (enable) {
			vTaskSuspend(taskHandle);
		} else {
			vTaskResume(taskHandle);
		}
	}

	void wakeupCheck() { }

	void exit() {
		mfrc522.PCD_SoftPowerDown();
		vTaskDelete(taskHandle);
	}

private:
	static void Task(void *ptr) {
		Mfrc522Driver *driver = static_cast<Mfrc522Driver *>(ptr);
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
			MFRC522::StatusCode result = driver->mfrc522.PICC_WakeupA(bufferATQA, &bufferSize);
			if (result == MFRC522::StatusCode::STATUS_OK) {
				// we found or woke up a card, read id
				driver->mfrc522.PICC_Select(&driver->mfrc522.uid, 0);
				cardAppliedCurrentRun = (result == MFRC522::StatusCode::STATUS_OK);

				// Bring card into HALT mode
				driver->mfrc522.PICC_HaltA();
				driver->mfrc522.PCD_StopCrypto1();
			}

			if (cardAppliedCurrentRun) {
				lastTimeCardDetect = millis();
				cardAppliedLastRun = true;

				CardIdType cardId;
				cardId.assign(driver->mfrc522.uid.uidByte);

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

				driver->signalEvent(msg);
			} else {
				if (!lastTimeCardDetect || (millis() - lastTimeCardDetect) > cardDetectTimeout) {
					// card was removed for sure
					lastTimeCardDetect = 0;
					if (cardAppliedLastRun) {
						// send the card removed event
						driver->signalEvent(Message::Event::CardRemoved, lastCardId);
					}
					cardAppliedLastRun = false;
					lastCardId = {};
				}
			}
		}
	}

	static constexpr uint32_t cardDetectTimeout = 200;

	enum class MainFsm : uint8_t {
		newCard = 0,
		locked,
		noCard
	};

	TaskHandle_t taskHandle {nullptr};

#if defined(RFID_READER_TYPE_MFRC522_I2C)
	MFRC522DriverI2C driver {MFRC522_ADDR, i2cBusTwo}; // Create I2C driver.
#elif defined(RFID_READER_TYPE_MFRC522_SPI)
	MFRC522DriverPinSimple ss_pin {MFRC522DriverPinSimple(RFID_CS)}; // Create pin driver. See typical pin layout above.
	MFRC522DriverSPI driver {MFRC522DriverSPI(ss_pin)}; // Create SPI driver.
#endif

	MFRC522 mfrc522 {MFRC522(driver)}; // Create MFRC522 instance.
};

} // namespace implementation

using RfidDriver = implementation::Mfrc522Driver;

} // namespace rfid::driver
