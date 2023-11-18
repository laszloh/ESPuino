#pragma once

#include <Arduino.h>
#include "settings.h"

#include <PN5180.h>
#include <PN5180ISO14443.h>
#include <PN5180ISO15693.h>

namespace rfid::driver {
namespace implementation {

class RfidPN1580 : public RfidDriverBase<RfidPN1580> {
	using RfidDriverBase::accessGuard;
	using RfidDriverBase::cardChangeEvent;
	using RfidDriverBase::message;
	using RfidDriverBase::signalEvent;

public:
	void init() {
#if defined(PN5180_ENABLE_LPCD)
		esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
		if (wakeupReason == ESP_SLEEP_WAKEUP_EXT1) {
			wakeupCheck();
		}
		// we are still here, so disable deep sleep
		gpio_deep_sleep_hold_dis();
		gpio_hold_dis(gpio_num_t(RFID_CS));
		gpio_hold_dis(gpio_num_t(RFID_RST));
	#if GPIO_IS_VALID_GPIO(RFID_IRQ)
		pinMode(RFID_IRQ, INPUT); // Not necessary for port-expander as for pca9555 all pins are configured as input per default
	#endif
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
	#if GPIO_IS_VALID_GPIO(RFID_IRQ)
		pinMode(RFID_IRQ, INPUT); // Not necessary for port-expander as for pca9555 all pins are configured as input per default
	#endif
#endif
		xTaskCreatePinnedToCore(
			RfidPN1580::Task,
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

	void wakeupCheck() {
		// disable pin hold from deep sleep
		gpio_deep_sleep_hold_dis();
		gpio_hold_dis(gpio_num_t(RFID_CS)); // NSS
		gpio_hold_dis(gpio_num_t(RFID_RST)); // RST
#if GPIO_IS_VALID_GPIO(RFID_IRQ)
		pinMode(RFID_IRQ, INPUT);
#endif
		nfc14443.begin();
		nfc14443.reset();
		// enable RF field
		nfc14443.setupRF();
		if (!nfc14443.isCardPresent()) {
			nfc14443.reset();
			constexpr uint16_t wakeupCounterInMs = 0x3FF; //  needs to be in the range of 0x0 - 0xA82. max wake-up time is 2960 ms.
			if (nfc14443.switchToLPCD(wakeupCounterInMs)) {
				Log_Println(lowPowerCardSuccess, LOGLEVEL_INFO);
// configure wakeup pin for deep-sleep wake-up, use ext1
#if GPIO_IS_VALID_GPIO(RFID_IRQ)
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

	void exit() {
#ifdef PN5180_ENABLE_LPCD
		setLpcdShutdownStatus(true);
		while (getLpcdShutdownStatus()) { // Make sure init of LPCD is complete!
			vTaskDelay(portTICK_PERIOD_MS * 10u);
		}
#endif
		vTaskDelete(taskHandle);
	}

	void setLpcdShutdownStatus(bool lpcdStatus) {
		enableLpcdShutdown = lpcdStatus;
	}

	bool getLpcdShutdownStatus(void) const {
		return enableLpcdShutdown;
	}

	void enableLpcd() {
		nfc14443.begin();
		nfc14443.reset();
		// show PN5180 reader version
		uint8_t firmwareVersion[2];
		nfc14443.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
		Log_Printf(LOGLEVEL_DEBUG, "PN5180 firmware version=%d.%d", firmwareVersion[1], firmwareVersion[0]);

		// check firmware version: PN5180 firmware < 4.0 has several bugs preventing the LPCD mode
		// you can flash latest firmware with this project: https://github.com/abidxraihan/PN5180_Updater_ESP32
		if (firmwareVersion[1] < 4) {
			Log_Println("This PN5180 firmware does not work with LPCD! use firmware >= 4.0", LOGLEVEL_ERROR);
			return;
		}
		Log_Println("prepare low power card detection...", LOGLEVEL_NOTICE);
		uint8_t irqConfig = 0b0000000; // Set IRQ active low + clear IRQ-register
		nfc14443.writeEEprom(IRQ_PIN_CONFIG, &irqConfig, 1);
		/*
		nfc.readEEprom(IRQ_PIN_CONFIG, &irqConfig, 1);
		Log_Printf("IRQ_PIN_CONFIG=0x%02X", irqConfig)
		*/
		nfc14443.prepareLPCD();
#if GPIO_IS_VALID_GPIO(RFID_IRQ)
		Log_Printf(LOGLEVEL_DEBUG, "PN5180 IRQ PIN (%d) state: %d", RFID_IRQ, Port_Read(RFID_IRQ));
#endif
		// turn on LPCD
		uint16_t wakeupCounterInMs = 0x3FF; //  must be in the range of 0x0 - 0xA82. max wake-up time is 2960 ms.
		if (nfc14443.switchToLPCD(wakeupCounterInMs)) {
			Log_Println("switch to low power card detection: success", LOGLEVEL_NOTICE);
			// configure wakeup pin for deep-sleep wake-up, use ext1. For a real GPIO only, not PE

#if GPIO_IS_VALID_GPIO(RFID_IRQ)
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

private:
	enum class MainFsm {
		init,
		nfc14443,
		nfc15693
	};

	enum class Nfc14443Fsm {
		reset,
		readcard,
	};

	enum class Nfc15693Fsm {
		reset,
		disablePrivacyMode,
		readcard,
	};

	static void Task(void *ptr) {
		RfidPN1580 *driver = static_cast<RfidPN1580 *>(ptr);
		MainFsm fsm = MainFsm::init;
		Nfc14443Fsm nfc14443Fsm = Nfc14443Fsm::reset;
		Nfc15693Fsm nfc15693Fsm = Nfc15693Fsm::reset;
		uint32_t lastTimeCardDetect = 0;
		Message::CardIdType lastCardId;
		bool cardAppliedLastRun = false;

		while (1) {
			bool cardReceived = false;
			Message::CardIdType cardId;

			vTaskDelay(portTICK_RATE_MS * 10u);
#ifdef PN5180_ENABLE_LPCD
			if (driver->getLpcdShutdownStatus()) {
				driver->enableLpcd();
				driver->setLpcdShutdownStatus(false); // give feedback that execution is complete
				while (true) {
					vTaskDelay(portTICK_PERIOD_MS * 100u); // there's no way back if shutdown was initiated
				}
			}
#endif

			switch (fsm) {
				case MainFsm::init: {
					driver->nfc14443.begin();
					driver->nfc14443.reset();
					// show PN1580 reader version
					uint8_t firmware[2];
					driver->nfc14443.readEEprom(FIRMWARE_VERSION, firmware, sizeof(firmware));
					Log_Printf(LOGLEVEL_DEBUG, "PN5180 firmware version=%d.%d", firmware[1], firmware[0]);

					// activate RF field
					delay(4);
					Log_Println(rfidScannerReady, LOGLEVEL_DEBUG);

					// start with NFC1443 detection
					fsm = MainFsm::nfc14443;
				} break;

				case MainFsm::nfc14443: {
					uint8_t uid[8];

					switch (nfc14443Fsm) {
						case Nfc14443Fsm::reset:
							driver->nfc14443.reset();
							nfc14443Fsm = Nfc14443Fsm::readcard;
							break;

						case Nfc14443Fsm::readcard:
							if (driver->nfc14443.readCardSerial(uid) >= 4) {
								cardReceived = true;
								std::copy(uid, uid + cardId.size(), cardId.begin());
								lastTimeCardDetect = millis();
							} else {
								// Reset to dummy-value if no card is there
								// Necessary to differentiate between "card is still applied" and "card is re-applied again after removal"
								// lastTimeCardDetect is used to prevent "new card detection with old card" with single events where no card was detected
								if (!lastTimeCardDetect || (millis() - lastTimeCardDetect) >= RfidPN1580::cardDetectTimeout) {
									lastTimeCardDetect = 0;
									lastCardId = {};

									// try nfc15693
									nfc14443Fsm = Nfc14443Fsm::reset;
									fsm = MainFsm::nfc15693;
								}
							}
							break;
					}
				} break;

				case MainFsm::nfc15693:
					switch (nfc15693Fsm) {
						case Nfc15693Fsm::reset:
							driver->nfc15693.reset();
							driver->nfc15693.setupRF();

							nfc15693Fsm = Nfc15693Fsm::readcard;
							break;

						case Nfc15693Fsm::disablePrivacyMode: {
							// we are in privacy mode, try to unlock first
							bool success = false;
							for (const auto e : rfidPassword) {
								ISO15693ErrorCode ret = driver->nfc15693.disablePrivacyMode((uint8_t *) e.data());
								if (ret == ISO15693_EC_OK) {
									success = true;
									Log_Printf(LOGLEVEL_NOTICE, rfid15693TagUnlocked);
									nfc15693Fsm = Nfc15693Fsm::readcard;
									break;
								}
							}
							if (!success) {
								// none of our password worked
								Log_Println(rfid15693TagUnlockFailed, LOGLEVEL_ERROR);

								nfc15693Fsm = Nfc15693Fsm::reset;
							}
						} break;

						case Nfc15693Fsm::readcard: {
							uint8_t uid[8];
							// try to read ISO15693 inventory
							ISO15693ErrorCode ret = driver->nfc15693.getInventory(uid);
							if (ret == ISO15693_EC_OK) {
								cardReceived = true;
								std::copy(uid, uid + cardId.size(), cardId.begin());
								lastTimeCardDetect = millis();
							} else if (ret == ISO15693_EC_BLOCK_IS_LOCKED) {
								// we have a locked chip, try to unlock
								nfc15693Fsm = Nfc15693Fsm::disablePrivacyMode;
							} else {
								// lastTimeDetected15693 is used to prevent "new card detection with old card" with single events where no card was detected
								if (!lastTimeCardDetect || (millis() - lastTimeCardDetect >= 400)) {
									lastTimeCardDetect = 0;
									lastCardId = {};

									// try nfc14443 next
									nfc15693Fsm = Nfc15693Fsm::reset;
									fsm = MainFsm::nfc14443;
								}
							}
						} break;
					}
					break;
			}

			if (cardReceived) {
				// check if it is the same card
				if (cardId == lastCardId) {
					// same card, reset reader
					nfc14443Fsm = Nfc14443Fsm::reset;
					nfc15693Fsm = Nfc15693Fsm::reset;
					continue;
				}
				lastCardId = cardId;

				Message msg;
				msg.event = Message::Event::CardApplied;
				msg.cardId = cardId;

				Log_Printf(LOGLEVEL_NOTICE, rfidTagDetected, msg.toHexString().c_str());
				Log_Printf(LOGLEVEL_NOTICE, "Card type: %s", (fsm == MainFsm::nfc14443) ? "ISO-14443" : "ISO-15693");

				driver->signalEvent(msg);
			} else if (!cardReceived && cardAppliedLastRun) {
				driver->signalEvent(Message::Event::CardRemoved);
				Log_Println(rfidTagRemoved, LOGLEVEL_NOTICE);
			} else {
				// signal card is still missing & reset state machines
				driver->signalEvent(Message::Event::NoCard);

				fsm = MainFsm::nfc14443;
				nfc14443Fsm = Nfc14443Fsm::reset;
				nfc15693Fsm = Nfc15693Fsm::reset;
			}

			cardAppliedLastRun = cardReceived;
		}
	}

	static constexpr uint32_t cardDetectTimeout = 1000;

	TaskHandle_t taskHandle {nullptr};
	bool enableLpcdShutdown {false};
	PN5180ISO14443 nfc14443 {PN5180ISO14443(RFID_CS, RFID_BUSY, RFID_RST)};
	PN5180ISO15693 nfc15693 {PN5180ISO15693(RFID_CS, RFID_BUSY, RFID_RST)};
};

} // namespace implementation

using RfidDriver = implementation::RfidPN1580;

} // namespace rfid::driver
