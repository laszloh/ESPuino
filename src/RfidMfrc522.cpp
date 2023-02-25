#include <Arduino.h>
#include "settings.h"
#include "Rfid.h"
#include "Log.h"
#include "MemX.h"
#include "Queues.h"
#include "System.h"
#include <esp_task_wdt.h>
#include "AudioPlayer.h"
#include "HallEffectSensor.h"
#include <FastLED.h>

#if defined RFID_READER_TYPE_MFRC522_SPI || defined RFID_READER_TYPE_MFRC522_I2C
	#ifdef RFID_READER_TYPE_MFRC522_SPI
		#include <MFRC522.h>
	#endif
	#if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE)
		#include "Wire.h"
	#endif
	#ifdef RFID_READER_TYPE_MFRC522_I2C
		#include <MFRC522_I2C.h>
	#endif

	static void Rfid_Task(void *parameter);

	#ifdef RFID_READER_TYPE_MFRC522_I2C
		extern TwoWire i2cBusTwo;
		static MFRC522_I2C mfrc522(MFRC522_ADDR, MFRC522_RST_PIN, &i2cBusTwo);
	#endif
	#ifdef RFID_READER_TYPE_MFRC522_SPI
		static MFRC522 mfrc522(RFID_CS, RST_PIN);
	#endif

	void Rfid_Init(void) {
		#ifdef RFID_READER_TYPE_MFRC522_SPI
			SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
			SPI.setFrequency(1000000);
		#endif

		// Init RC522 Card-Reader
		mfrc522.PCD_SetAntennaGain(rfidGain);
		mfrc522.PCD_Init();
		mfrc522.PCD_AntennaOn();
		delay(50);
		Log_Println((char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);

		xTaskCreatePinnedToCore(
			Rfid_Task,              /* Function to implement the task */
			"rfid",                 /* Name of the task */
			2048,                   /* Stack size in words */
			NULL,                   /* Task input parameter */
			2 | portPRIVILEGE_BIT,  /* Priority of the task */
			NULL,                   /* Task handle. */
			1                       /* Core where the task should run */
		);
	}

	void Rfid_Task(void *parameter) {
		bool cardApplied = false;
		uint8_t lastCardId[cardIdSize] = {0};

		for (;;) {
			if (RFID_SCAN_INTERVAL/2 >= 20) {
				vTaskDelay(portTICK_RATE_MS * (RFID_SCAN_INTERVAL/2));
			} else {
			   vTaskDelay(portTICK_RATE_MS * 20);
			}
			uint8_t cardId[cardIdSize];

			static CEveryNMillis checkInterval(RFID_SCAN_INTERVAL);
			if (checkInterval) {
				//snprintf(Log_Buffer, Log_BufferLength, "%u", uxTaskGetStackHighWaterMark(NULL));
				//Log_Println(Log_Buffer, LOGLEVEL_DEBUG);

				// Reset the loop if no new card is present on the sensor/reader. This saves the entire process when idle.
				RfidMessage msg = {RfidEvent::NoCard};
				MFRC522::Uid uid;
				bool cardFound = false;
				
				for(uint8_t retry=0;retry<6;retry++) {
					// try multiple times to find a card
					uint8_t bufferATQA[2];
					uint8_t bufferSize = sizeof(bufferATQA);

					// Reset baud rates
					mfrc522.PCD_WriteRegister(MFRC522::TxModeReg, 0x00);
					mfrc522.PCD_WriteRegister(MFRC522::RxModeReg, 0x00);
					// Reset ModWidthReg
					mfrc522.PCD_WriteRegister(MFRC522::ModWidthReg, 0x26);

					MFRC522::StatusCode ret = mfrc522.PICC_WakeupA(bufferATQA, &bufferSize);
					// log_v("ret: %d", ret);
					if(ret != MFRC522::STATUS_OK && ret != MFRC522::STATUS_COLLISION){
						continue;
					}

					ret = mfrc522.PICC_Select(&uid);
					// log_v("ret: %d", ret);
					if(ret == MFRC522::STATUS_OK) {
						cardFound = true;
						break;
					}
				}
				if(!cardFound) {
					// we did not find a card
					if(cardApplied){
						// card was taken away
						Log_Println((char *) FPSTR(rfidTagRemoved), LOGLEVEL_NOTICE);
						msg.event = RfidEvent::CardRemoved;
						xQueueSend(gRfidCardQueue, &msg, 0);
					}
					cardApplied = false;

					memset(lastCardId, 0, cardIdSize);
					mfrc522.PICC_HaltA();
					mfrc522.PCD_StopCrypto1();
					continue;
				}
				// log_v("cf: %d", cardFound);
				mfrc522.PICC_HaltA();
				mfrc522.PCD_StopCrypto1();

				memcpy(cardId, uid.uidByte, cardIdSize);
    			#ifdef HALLEFFECT_SENSOR_ENABLE
					cardId[cardIdSize-1]   = cardId[cardIdSize-1] + gHallEffectSensor.waitForState(HallEffectWaitMS);  
				#endif

				if(memcmp(lastCardId, cardId, cardIdSize) != 0) {
					Log_Print((char *) FPSTR(rfidTagDetected), LOGLEVEL_NOTICE, true);
					for (uint8_t i=0u; i < cardIdSize; i++) {
						snprintf(Log_Buffer, Log_BufferLength, "%02x%s", cardId[i], (i < cardIdSize - 1u) ? "-" : "\n");
						Log_Print(Log_Buffer, LOGLEVEL_NOTICE, false);
					}

					// different card id read
					msg.event = RfidEvent::CardApplied;
					memcpy(msg.cardId, cardId, cardIdSize);
					xQueueSend(gRfidCardQueue, &msg, 0);
				}
				memcpy(lastCardId, cardId, cardIdSize);

				cardApplied = true;
			}
		}
	}

	void Rfid_Cyclic(void) {
		// Not necessary as cyclic stuff performed by task Rfid_Task()
	}

	void Rfid_Exit(void) {
		#ifndef RFID_READER_TYPE_MFRC522_I2C
			mfrc522.PCD_SoftPowerDown();
		#endif
	}

	void Rfid_WakeupCheck(void) {
	}

#endif
