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

#if defined(RFID_READER_TYPE_PN532_I2C) || defined(RFID_READER_TYPE_PN532_SPI)

#ifdef RFID_READER_TYPE_PN532_SPI
	#include <SPI.h>
#endif
#if defined(RFID_READER_TYPE_PN532_I2C) || defined(PORT_EXPANDER_ENABLE)
	#include <Wire.h>
#endif
#include "Adafruit_PN532.h"

#ifdef RFID_READER_TYPE_PN532_I2C
	static Adafruit_PN532 pn532(RFID_IRQ, RFID_RST);
#endif
#ifdef RFID_READER_TYPE_PN532_SPI
	static Adafruit_PN532 pn532(RFID_CS);
#endif

static void Rfid_Task(void *p);
static TaskHandle_t rfidTaskHandle;

void Rfid_Init(void) {
	#ifdef PN532_ENABLE_LPCD
		// Check if wakeup-reason was card-detection (PN5180 only)
		// This only works if RFID.IRQ is connected to a GPIO and not to a port-expander
		esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
		if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
			Rfid_WakeupCheck();
		}
		// disable pin hold from deep sleep
		gpio_deep_sleep_hold_dis();
		gpio_hold_dis(gpio_num_t(RFID_CS));  // NSS
		gpio_hold_dis(gpio_num_t(RFID_RST)); // RST
		#if (RFID_IRQ >= 0 && RFID_IRQ <= MAX_GPIO)
			pinMode(RFID_IRQ, INPUT);       // Not necessary for port-expander as for pca9555 all pins are configured as input per default
		#endif
	#endif

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
	uint8_t uid[7];
	uint8_t length;

	while(1) {
		bool success = pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &length);

		if(success) {
			// we got a uid
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