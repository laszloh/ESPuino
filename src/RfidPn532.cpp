#include <Arduino.h>
#include "settings.h"

#include "AudioPlayer.h"
#include "HallEffectSensor.h"
#include "Log.h"
#include "MemX.h"
#include "Power.h"
#include "Queues.h"
#include "Rfid.h"
#include "System.h"
#include "Wire.h"

#if defined(RFID_READER_TYPE_PN532)

TaskHandle_t rfidTaskHandle;
static void Rfid_Task(void *parameter);
	#if defined(INTERFACE_I2C)
		#include <PN532_I2C.h>
		#include <Wire.h>

PN532_I2C pn532interface(rfidI2C);
	#elif defined(INTERFACE_SPI)
		#include <PN532_SPI.h>
		#include <SPI.h>

PN532_SPI pn532interface(SPI, RFID_CS);
	#endif

	#include <PN532.h>
PN532 pn532(pn532interface);

static int I2C_ClearBus(uint8_t sda, uint8_t scl) {
	pinMode(sda, INPUT_PULLUP); // Make SDA (data) and SCL (clock) pins Inputs with pullup.
	pinMode(scl, INPUT_PULLUP);

	delay(2500); // Wait 2.5 secs. This is strictly only necessary on the first power
	// up of the DS3231 module to allow it to initialize properly,
	// but is also assists in reliable programming of FioV3 boards as it gives the
	// IDE a chance to start uploaded the program
	// before existing sketch confuses the IDE by sending Serial data.

	boolean SCL_LOW = (digitalRead(scl) == LOW); // Check is SCL is Low.
	if (SCL_LOW) { // If it is held low Arduno cannot become the I2C master.
		return 1; // I2C bus error. Could not clear SCL clock line held low
	}

	boolean SDA_LOW = (digitalRead(sda) == LOW); // vi. Check SDA input.
	int clockCount = 20; // > 2x9 clock

	while (SDA_LOW && (clockCount > 0)) { //  vii. If SDA is Low,
		clockCount--;
		// Note: I2C bus is open collector so do NOT drive SCL or SDA high.
		pinMode(scl, INPUT); // release SCL pullup so that when made output it will be LOW
		pinMode(scl, OUTPUT); // then clock SCL Low
		delayMicroseconds(10); //  for >5us
		pinMode(scl, INPUT); // release SCL LOW
		pinMode(scl, INPUT_PULLUP); // turn on pullup resistors again
		// do not force high as slave may be holding it low for clock stretching.
		delayMicroseconds(10); //  for >5us
		// The >5us is so that even the slowest I2C devices are handled.
		SCL_LOW = (digitalRead(scl) == LOW); // Check if SCL is Low.
		int counter = 20;
		while (SCL_LOW && (counter > 0)) { //  loop waiting for SCL to become High only wait 2sec.
			counter--;
			delay(100);
			SCL_LOW = (digitalRead(scl) == LOW);
		}
		if (SCL_LOW) { // still low after 2 sec error
			return 2; // I2C bus error. Could not clear. SCL clock line held low by slave clock stretch for >2sec
		}
		SDA_LOW = (digitalRead(sda) == LOW); //   and check SDA input again and loop
	}
	if (SDA_LOW) { // still low
		return 3; // I2C bus error. Could not clear. SDA data line held low
	}

	// else pull SDA line low for Start or Repeated Start
	pinMode(sda, INPUT); // remove pullup.
	pinMode(sda, OUTPUT); // and then make it LOW i.e. send an I2C Start or Repeated start control.
	// When there is only one I2C master a Start or Repeat Start has the same function as a Stop and clears the bus.
	/// A Repeat Start is a Start occurring after a Start with no intervening Stop.
	delayMicroseconds(10); // wait >5us
	pinMode(sda, INPUT); // remove output low
	pinMode(sda, INPUT_PULLUP); // and make SDA high i.e. send I2C STOP control.
	delayMicroseconds(10); // x. wait >5us
	pinMode(sda, INPUT); // and reset pins as tri-state inputs which is the default state on reset
	pinMode(scl, INPUT);
	return 0; // all ok
}

void resetAndInit() {
	#if defined(INTERFACE_I2C)
	rfidI2C.end();
	Power_PeripheralOff();
	delay(100);
	Power_PeripheralOn();
	delay(100);
	I2C_ClearBus(I2C1_SDA, I2C1_SCL);
	rfidI2C.begin(I2C1_SDA, I2C1_SCL, 10'000);
	#endif

	pn532.begin();
	pn532.setPassiveActivationRetries(0x02);
	pn532.SAMConfig();
}

void Rfid_Driver_Init(void) {
	#ifdef INTERFACE_SPI
	SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
	SPI.setFrequency(100000);
	#elif defined(INTERFACE_I2C)
	Power_PeripheralOff();
	delay(20);
	Power_PeripheralOn();
	delay(10);
	I2C_ClearBus(I2C1_SDA, I2C1_SCL);
	rfidI2C.begin(I2C1_SDA, I2C1_SCL, 10'000);
	#endif
	resetAndInit();

	const uint32_t version = pn532.getFirmwareVersion();
	if (!version) {
		Log_Println("Did not find NFC card reader!", LOGLEVEL_ERROR);
		return;
	}
	Log_Printf(LOGLEVEL_NOTICE, "Found PN5%X FW: %d.%d", (version >> 24) & 0xFF, (version >> 16) & 0xFF, (version >> 8) & 0xFF);

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
			resetAndInit();
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
}

void Rfid_WakeupCheck(void) {
}

#endif
