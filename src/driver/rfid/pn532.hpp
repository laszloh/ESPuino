#pragma once

#include <Arduino.h>
#include "settings.h"

#include "Log.h"
#include "SPI.h"
#include "Wire.h"

#include <PN532.h>
#include <PN532_I2C.h>
#include <PN532_SPI.h>

extern TwoWire i2cBusTwo;

namespace rfid::driver {
namespace implementation {

class PN532Driver : public RfidDriverBase<PN532Driver> {
	using RfidDriverBase::accessGuard;
	using RfidDriverBase::cardChangeEvent;
	using RfidDriverBase::message;
	using RfidDriverBase::signalEvent;

public:
	void init() {
#ifdef RFID_READER_TYPE_PN532_SPI
		SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
		SPI.setFrequency(1000000);
#endif
		reset();
		pn532.begin();

		const uint32_t version = pn532.getFirmwareVersion();
		if (!version) {
			Log_Println("Did not find NFC card reader!", LOGLEVEL_ERROR);
			return;
		}
		Log_Printf(LOGLEVEL_NOTICE, "Found PN5%X FW: %d.%d\n", (version >> 24) & 0xFF, (version >> 16) & 0xFF, (version >> 8) & 0xFF);
		pn532.setPassiveActivationRetries(0x05);
		pn532.SAMConfig();

		Log_Println(rfidScannerReady, LOGLEVEL_DEBUG);

		xTaskCreatePinnedToCore(
			PN532Driver::Task,
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
		pn532.powerDownMode();
		vTaskDelete(taskHandle);
	}

private:
	static void Task(void *ptr) {
		PN532Driver *driver = static_cast<PN532Driver *>(ptr);
		uint32_t lastTimeCardDetect = 0;
		Message::CardIdType lastCardId;
		bool cardAppliedLastRun = false;

		while (1) {
			uint8_t uid[10];
			uint8_t uidLen;
			Message::CardIdType cardId;

			if constexpr (RFID_SCAN_INTERVAL / 2 >= 20) {
				vTaskDelay(portTICK_PERIOD_MS * (RFID_SCAN_INTERVAL / 2));
			} else {
				vTaskDelay(portTICK_PERIOD_MS * 20);
			}

			bool cardAppliedCurrentRun = driver->pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 5);

			if (cardAppliedCurrentRun) {
				lastTimeCardDetect = millis();
				cardAppliedLastRun = true;
				std::copy(uid, uid + cardId.size(), cardId.begin());

				if (cardId == lastCardId) {
					// this is the same card
					continue;
				}
				lastCardId = cardId;

				// different card id read
				Message msg;
				msg.event = Message::Event::CardApplied;
				msg.cardId = cardId;

				Log_Printf(LOGLEVEL_NOTICE, rfidTagDetected, msg.toHexString().c_str());
				driver->signalEvent(msg);
			} else {
				if (!lastTimeCardDetect || (millis() - lastTimeCardDetect) > cardDetectTimeout) {
					// card was removed for sure
					lastTimeCardDetect = 0;
					if (cardAppliedLastRun) {
						// send the card removed event
						driver->signalEvent(Message::Event::CardRemoved);
					}
					cardAppliedLastRun = false;
					lastCardId = {};
				}
			}
		}
	}

	void reset() const {
		if constexpr (rstPin < GPIO_PIN_COUNT && GPIO_IS_VALID_GPIO(rstPin)) {
			pinMode(rstPin, OUTPUT);
			digitalWrite(rstPin, LOW);
			delayMicroseconds(100);
			digitalWrite(rstPin, HIGH);
			delay(10);
		}
	}

	static constexpr uint32_t cardDetectTimeout = 200;
	static constexpr size_t rstPin = RST_PIN;

	TaskHandle_t taskHandle {nullptr};

#if defined(RFID_READER_TYPE_PN532_I2C)
	PN532_I2C driver {NP532_I2C(i2cBusTwo)};
#elif defined(RFID_READER_TYPE_PN532_SPI)
	PN532_SPI driver {PN532_SPI(SPI, RFID_CS)};
#endif
	PN532 pn532 {PN532(driver)}; // Create MFRC522 instance.
};

} // namespace implementation

using RfidDriver = implementation::PN532Driver;

} // namespace rfid::driver
