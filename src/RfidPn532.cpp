#include <Arduino.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include "settings.h"
#include "Rfid.h"
#include "Log.h"
#include "MemX.h"
#include "Queues.h"
#include "System.h"
#include "Port.h"
#include <esp_task_wdt.h>
#include "AudioPlayer.h"
#include <FastLED.h>

#if defined(RFID_READER_TYPE_PN532_I2C) || defined(RFID_READER_TYPE_PN532_SPI)

#ifdef RFID_READER_TYPE_PN532_SPI
	#include <SPI.h>
	#include <PN532_SPI.h>

	PN532_SPI pn532interface(SPI, RFID_CS);
#endif
#if defined(RFID_READER_TYPE_PN532_I2C)
	#include <Wire.h>
	#include <PN532_I2C.h>

	PN532_I2C pn532interface(Wire);
#endif

#include <PN532.h>
PN532 nfc(pn532interface);

static void Rfid_Task(void *p);
static TaskHandle_t rfidTaskHandle;

static int I2C_ClearBus( uint8_t sda, uint8_t scl) {
#if defined(TWCR) && defined(TWEN)
  TWCR &= ~(_BV(TWEN)); //Disable the Atmel 2-Wire interface so we can control the SDA and SCL pins directly
#endif

  pinMode(sda, INPUT_PULLUP); // Make SDA (data) and SCL (clock) pins Inputs with pullup.
  pinMode(scl, INPUT_PULLUP);

  delay(2500);  // Wait 2.5 secs. This is strictly only necessary on the first power
  // up of the DS3231 module to allow it to initialize properly,
  // but is also assists in reliable programming of FioV3 boards as it gives the
  // IDE a chance to start uploaded the program
  // before existing sketch confuses the IDE by sending Serial data.

  boolean SCL_LOW = (digitalRead(scl) == LOW); // Check is SCL is Low.
  if (SCL_LOW) { //If it is held low Arduno cannot become the I2C master. 
    return 1; //I2C bus error. Could not clear SCL clock line held low
  }

  boolean SDA_LOW = (digitalRead(sda) == LOW);  // vi. Check SDA input.
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
    while (SCL_LOW && (counter > 0)) {  //  loop waiting for SCL to become High only wait 2sec.
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
  pinMode(sda, OUTPUT);  // and then make it LOW i.e. send an I2C Start or Repeated start control.
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
	digitalWrite(RFID_RST, LOW);
	delayMicroseconds(1);
	digitalWrite(RFID_RST, HIGH);
	delay(10);
	
	nfc.begin();
	nfc.setPassiveActivationRetries(0x05);
	nfc.SAMConfig();
}

void Rfid_Init(void) {

	if(RFID_RST <= GPIO_NUM_MAX){
		pinMode(RFID_RST, OUTPUT);
		digitalWrite(RFID_RST, LOW);
		delayMicroseconds(100);
		digitalWrite(RFID_RST, HIGH);
		delay(10);
	}

	I2C_ClearBus(I2C1_SDA, I2C1_SCL);
	Wire.begin(I2C1_SDA, I2C1_SCL, 10000u);
	nfc.begin();

	uint32_t versiondata = nfc.getFirmwareVersion();
	if (!versiondata)
		Log_Println("Did not find NFC card!", LOGLEVEL_ERROR);
	// Got ok data, print it out!
	snprintf(Log_Buffer, Log_BufferLength, "Found PN5%X FW: %d.%d\n", (versiondata>>24) & 0xFF, (versiondata>>16) & 0xFF, (versiondata>>8) & 0xFF);
	Log_Print(Log_Buffer, LOGLEVEL_NOTICE, false);

	resetAndInit();

	xTaskCreatePinnedToCore(
		Rfid_Task,              /* Function to implement the task */
		"rfid",                 /* Name of the task */
		2048,                   /* Stack size in words */
		NULL,                   /* Task input parameter */
		2 | portPRIVILEGE_BIT,  /* Priority of the task */
		&rfidTaskHandle,        /* Task handle. */
		0                       /* Core where the task should run */
	);
}

void Rfid_Task(void *p) {
	uint8_t uid[10];
	uint8_t uidLength;
	uint32_t lastTimeDetected14443 = 0;
	uint8_t lastCardId[cardIdSize];
	bool cardAppliedLastRun = false;

	while(1) {
		RfidMessage msg = {RfidEvent::NoCard};

		if(RFID_SCAN_INTERVAL/2 >= 20) {
			vTaskDelay(portTICK_RATE_MS * (RFID_SCAN_INTERVAL/2));
		} else {
			vTaskDelay(portTICK_RATE_MS * 20);
		}

		bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 5);
		if(success) {
			// we have a card in the RF field
			lastTimeDetected14443 = millis();
			cardAppliedLastRun = true;

			if(memcmp(uid, lastCardId, cardIdSize) != 0) {
				// different card id read
				Log_Print((char *) FPSTR(rfidTagDetected), LOGLEVEL_NOTICE, true);
				for (uint8_t i=0u; i < cardIdSize; i++) {
					snprintf(Log_Buffer, Log_BufferLength, "%02x%s", uid[i], (i < cardIdSize - 1u) ? "-" : "\n");
					Log_Print(Log_Buffer, LOGLEVEL_NOTICE, false);
				}

				// different card id read
				msg.event = RfidEvent::CardApplied;
				memcpy(msg.cardId, uid, cardIdSize);
				xQueueSend(gRfidCardQueue, &msg, 0);
			}
			memcpy(lastCardId, uid, cardIdSize);
		} else {
			if(!lastTimeDetected14443 || (millis() - lastTimeDetected14443) > 200) {
				// card was removed for sure
				lastTimeDetected14443 = 0;

				if (cardAppliedLastRun) {   // Card was just removed => send to queue
					msg.event = RfidEvent::CardRemoved;
					memcpy(msg.cardId, lastCardId, cardIdSize);
					xQueueSend(gRfidCardQueue, &msg, 0);
					resetAndInit();
				}
				cardAppliedLastRun = false;

				memset(lastCardId, 0, cardIdSize);
				memset(uid, 0, sizeof(uid));
			}
		}
	}
}

void Rfid_Cyclic(void) {
	// Not necessary as cyclic stuff performed by task Rfid_Task()
}

void Rfid_Exit(void) {
	#ifndef RFID_READER_TYPE_PN532_I2C
		// power down the NFC reader
	#endif
}

void Rfid_WakeupCheck(void) {
}

#endif