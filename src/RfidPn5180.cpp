#include <Arduino.h>
#include "settings.h"

#include "AudioPlayer.h"
#include "HallEffectSensor.h"
#include "Log.h"
#include "MemX.h"
#include "Port.h"
#include "Queues.h"
#include "Rfid.h"
#include "System.h"

#include <Wire.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include <freertos/task.h>

#ifdef RFID_READER_TYPE_PN5180
	#include <PN5180.h>
	#include <PN5180ISO14443.h>
	#include <PN5180ISO15693.h>

enum class MainFsm : uint8_t {
	Nfc14443,
	Nfc15693
};

enum class Nfc14443Fsm : uint8_t {
	Reset,
	ReadCard
};

enum class Nfc15693Fsm : uint8_t {
	Reset,
	GetInventory,
	DisablePrivacyMode,
};

	#if (defined(PORT_EXPANDER_ENABLE) && (RFID_IRQ > 99))
extern TwoWire i2cBusTwo;
	#endif

static void Rfid_Task(void *parameter);
TaskHandle_t rfidTaskHandle;

void Rfid_EnableLpcd(void);
bool enabledLpcdShutdown __attribute__((unused)) = false; // Indicates if LPCD should be activated as part of the shutdown-process

void Rfid_SetLpcdShutdownStatus(bool lpcdStatus) {
	enabledLpcdShutdown = lpcdStatus;
}

bool Rfid_GetLpcdShutdownStatus(void) {
	return enabledLpcdShutdown;
}

void Rfid_Driver_Init(void) {
	#ifdef PN5180_ENABLE_LPCD
	// Check if wakeup-reason was card-detection (PN5180 only)
	// This only works if RFID.IRQ is connected to a GPIO and not to a port-expander
	esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
	if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
		Rfid_WakeupCheck();
	}

		// wakeup-check if IRQ is connected to port-expander, signal arrives as pushbutton
		#if (defined(PORT_EXPANDER_ENABLE) && (RFID_IRQ > 99))
	if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
		// read IRQ state from port-expander
		i2cBusTwo.begin(ext_IIC_DATA, ext_IIC_CLK);
		delay(50);
		Port_Init();
		uint8_t irqState = Port_Read(RFID_IRQ);
		if (irqState == LOW) {
			Log_Println("Wakeup caused by low power card-detection on port-expander", LOGLEVEL_NOTICE);
			Rfid_WakeupCheck();
		}
	}
		#endif

	// disable pin hold from deep sleep
	gpio_deep_sleep_hold_dis();
	gpio_hold_dis(gpio_num_t(RFID_CS)); // NSS
	gpio_hold_dis(gpio_num_t(RFID_RST)); // RST
		#if (RFID_IRQ < MAX_GPIO) && GPIO_IS_VALID_GPIO(RFID_IRQ)
	pinMode(RFID_IRQ, INPUT); // Not necessary for port-expander as for pca9555 all pins are configured as input per default
		#endif
	#endif

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
	MainFsm stateMachine = MainFsm::Nfc14443;

	PN5180ISO14443 nfc14443(RFID_CS, RFID_BUSY, RFID_RST);
	Nfc14443Fsm nfc14443Fsm = Nfc14443Fsm::Reset;

	PN5180ISO15693 nfc15693(RFID_CS, RFID_BUSY, RFID_RST);
	Nfc15693Fsm nfc15693Fsm = Nfc15693Fsm::Reset;
	bool nfc15693Unlocked = false;

	CardIdType lastCardId;
	bool cardAppliedLastRun = false;
	uint32_t lastTimeCardDetect = 0;

	nfc14443.begin();
	nfc14443.reset();
	// show PN5180 reader version
	uint8_t firmwareVersion[2];
	nfc14443.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
	Log_Printf(LOGLEVEL_DEBUG, "PN5180 firmware version=%d.%d", firmwareVersion[1], firmwareVersion[0]);

	// activate RF field
	delay(4u);
	Log_Println(rfidScannerReady, LOGLEVEL_DEBUG);

	for (;;) {
		uint8_t uid[10];
		bool cardAppliedCurrentRun = false;

		vTaskDelay(portTICK_PERIOD_MS * 10u);
	#ifdef PN5180_ENABLE_LPCD
		if (Rfid_GetLpcdShutdownStatus()) {
			Rfid_EnableLpcd();
			Rfid_SetLpcdShutdownStatus(false); // give feedback that execution is complete
			while (true) {
				vTaskDelay(portTICK_PERIOD_MS * 100u); // there's no way back if shutdown was initiated
			}
		}
	#endif

		switch (stateMachine) {
			case MainFsm::Nfc14443:
				switch (nfc14443Fsm) {
					case Nfc14443Fsm::Reset:
						nfc14443.reset();
						nfc14443.setupRF();
						nfc14443Fsm = Nfc14443Fsm::ReadCard;
						break;

					case Nfc14443Fsm::ReadCard:
						cardAppliedCurrentRun = (nfc14443.readCardSerial(uid) >= 4);
						// we do not nned to react to a missing card, that will be done below
						break;
				}
				break;

			case MainFsm::Nfc15693:
				switch (nfc15693Fsm) {
					case Nfc15693Fsm::Reset:
						nfc15693.reset();
						nfc15693.setupRF();
						nfc15693Unlocked = false;
						nfc15693Fsm = Nfc15693Fsm::GetInventory;
						break;

					case Nfc15693Fsm::GetInventory: {
						// try to read ISO15693 inventory
						ISO15693ErrorCode rc = nfc15693.getInventory(uid);
						if (rc == ISO15693_EC_OK) {
							cardAppliedCurrentRun = true;
							nfc15693Unlocked = true; // we could talk with the chip, so do not try to unlock it
						} else {
							if (!nfc15693Unlocked) {
								// we have not yet tried to unlock the card, do it NOW
								nfc15693Fsm = Nfc15693Fsm::DisablePrivacyMode;
								cardAppliedCurrentRun = true;
							}
							// we do not have to react to a missing card, that will be done below
						}
					} break;

					case Nfc15693Fsm::DisablePrivacyMode:
						// check for ICODE-SLIX2 password protected tag
						for (const auto pwd : slix2Passwords) {
							const ISO15693ErrorCode ret = nfc15693.disablePrivacyMode(pwd);
							if (ret == ISO15693_EC_OK) {
								// we unlocked it or no provacy mode active
								Log_Printf(LOGLEVEL_NOTICE, "disabling privacy-mode successful with passord: %02X-%02X-%02X-%02X\n", pwd[0], pwd[1], pwd[2], pwd[3]);
								nfc15693Fsm = Nfc15693Fsm::GetInventory;
								nfc15693Unlocked = true;
								break;
							}
						}
						if (!nfc15693Unlocked) {
							// we failed to unlock the tag with all passwords
							Log_Println("Failed to unlock the Tag with all passwords", LOGLEVEL_ERROR);

							// try an ISO14443 next
							stateMachine = MainFsm::Nfc14443;
							nfc15693Fsm = Nfc15693Fsm::Reset;
							nfc15693Unlocked = false;
						}
						break;
				}
				break;
		}

		if (cardAppliedCurrentRun) {
			// card is on the reader
			lastTimeCardDetect = millis();
			cardAppliedLastRun = true;

			CardIdType cardId;
			cardId.assign(uid);

			if (cardId == lastCardId) {
				// this is the same card
				continue;
			}

			// different card id read
			Message msg;
			msg.event = Message::Event::CardApplied;
			msg.cardId = cardId;

			Log_Printf(LOGLEVEL_NOTICE, rfidTagDetected, cardId.toHexString().c_str());
			Log_Printf(LOGLEVEL_NOTICE, "Card type: %s", (stateMachine == MainFsm::Nfc14443) ? "ISO-14443" : "ISO-15693");
			lastCardId = cardId;

			Rfid_SignalEvent(msg);
		} else {
			// card is not present
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

				// change the FSM to the other card
				nfc14443Fsm = Nfc14443Fsm::Reset;
				nfc15693Fsm = Nfc15693Fsm::Reset;
				stateMachine = (stateMachine == MainFsm::Nfc14443) ? MainFsm::Nfc15693 : MainFsm::Nfc14443;
			}
		}
	}
}

void Rfid_Exit(void) {
	#ifdef PN5180_ENABLE_LPCD
	Rfid_SetLpcdShutdownStatus(true);
	while (Rfid_GetLpcdShutdownStatus()) { // Make sure init of LPCD is complete!
		vTaskDelay(portTICK_PERIOD_MS * 10u);
	}
	#endif
	vTaskDelete(rfidTaskHandle);
}

// Handles activation of LPCD (while shutdown is in progress)
void Rfid_EnableLpcd(void) {
	// goto low power card detection mode
	PN5180 nfc(RFID_CS, RFID_BUSY, RFID_RST);
	nfc.begin();
	nfc.reset();
	// show PN5180 reader version
	uint8_t firmwareVersion[2];
	nfc.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
	Log_Printf(LOGLEVEL_DEBUG, "PN5180 firmware version=%d.%d", firmwareVersion[1], firmwareVersion[0]);

	// check firmware version: PN5180 firmware < 4.0 has several bugs preventing the LPCD mode
	// you can flash latest firmware with this project: https://github.com/abidxraihan/PN5180_Updater_ESP32
	if (firmwareVersion[1] < 4) {
		Log_Println("This PN5180 firmware does not work with LPCD! use firmware >= 4.0", LOGLEVEL_ERROR);
		return;
	}
	Log_Println("prepare low power card detection...", LOGLEVEL_NOTICE);
	uint8_t irqConfig = 0b0000000; // Set IRQ active low + clear IRQ-register
	nfc.writeEEprom(IRQ_PIN_CONFIG, &irqConfig, 1);
	/*
	nfc.readEEprom(IRQ_PIN_CONFIG, &irqConfig, 1);
	Log_Printf("IRQ_PIN_CONFIG=0x%02X", irqConfig)
	*/
	nfc.prepareLPCD();
	Log_Printf(LOGLEVEL_DEBUG, "PN5180 IRQ PIN (%d) state: %d", RFID_IRQ, Port_Read(RFID_IRQ));
	// turn on LPCD
	uint16_t wakeupCounterInMs = 0x3FF; //  must be in the range of 0x0 - 0xA82. max wake-up time is 2960 ms.
	if (nfc.switchToLPCD(wakeupCounterInMs)) {
		Log_Println("switch to low power card detection: success", LOGLEVEL_NOTICE);
	// configure wakeup pin for deep-sleep wake-up, use ext1. For a real GPIO only, not PE
	#if (RFID_IRQ < MAX_GPIO) && GPIO_IS_VALID_GPIO(RFID_IRQ)
		if (ESP_ERR_INVALID_ARG == esp_sleep_enable_ext1_wakeup((1ULL << (RFID_IRQ)), ESP_EXT1_WAKEUP_ALL_LOW)) {
			Log_Printf(LOGLEVEL_ERROR, wrongWakeUpGpio, RFID_IRQ);
		}
	#endif
		// freeze pin states in deep sleep
		gpio_hold_en(gpio_num_t(RFID_CS)); // CS/NSS
		gpio_hold_en(gpio_num_t(RFID_RST)); // RST
		gpio_deep_sleep_hold_en();
	} else {
		Log_Println("switchToLPCD failed", LOGLEVEL_ERROR);
	}
}

// wake up from LPCD, check card is present. This works only for ISO-14443 compatible cards
void Rfid_WakeupCheck(void) {
	// disable pin hold from deep sleep
	gpio_deep_sleep_hold_dis();
	gpio_hold_dis(gpio_num_t(RFID_CS)); // NSS
	gpio_hold_dis(gpio_num_t(RFID_RST)); // RST
	#if (RFID_IRQ < MAX_GPIO) && GPIO_IS_VALID_GPIO(RFID_IRQ)
	pinMode(RFID_IRQ, INPUT);
	#endif
	static PN5180ISO14443 nfc14443(RFID_CS, RFID_BUSY, RFID_RST);
	nfc14443.begin();
	nfc14443.reset();
	// enable RF field
	nfc14443.setupRF();
	if (!nfc14443.isCardPresent()) {
		nfc14443.reset();
		uint16_t wakeupCounterInMs = 0x3FF; //  needs to be in the range of 0x0 - 0xA82. max wake-up time is 2960 ms.
		if (nfc14443.switchToLPCD(wakeupCounterInMs)) {
			Log_Println(lowPowerCardSuccess, LOGLEVEL_INFO);
	// configure wakeup pin for deep-sleep wake-up, use ext1
	#if (RFID_IRQ < MAX_GPIO) && GPIO_IS_VALID_GPIO(RFID_IRQ)
			// configure wakeup pin for deep-sleep wake-up, use ext1. For a real GPIO only, not PE
			esp_sleep_enable_ext1_wakeup((1ULL << (RFID_IRQ)), ESP_EXT1_WAKEUP_ALL_LOW);
	#endif
	#if (defined(PORT_EXPANDER_ENABLE) && (RFID_IRQ > 99))
			// reset IRQ state on port-expander
			Port_Exit();
	#endif
			// freeze pin states in deep sleep
			gpio_hold_en(gpio_num_t(RFID_CS)); // CS/NSS
			gpio_hold_en(gpio_num_t(RFID_RST)); // RST
			gpio_deep_sleep_hold_en();
			Log_Println(wakeUpRfidNoIso14443, LOGLEVEL_ERROR);
			esp_deep_sleep_start();
		} else {
			Log_Println("switchToLPCD failed", LOGLEVEL_ERROR);
		}
	}
	nfc14443.end();
}

#endif // RFID_READER_TYPE_PN5180
