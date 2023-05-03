#pragma once

#include <PN5180.h>
#include <PN5180ISO14443.h>
#include <PN5180ISO15693.h>

namespace rfid::driver
{
namespace implementation
{

class RfidPN1580 : public RfidDriverBase<RfidPN1580> {
	using RfidDriverBase::cardChangeEvent;
	using RfidDriverBase::message;
	using RfidDriverBase::accessGuard;
	using RfidDriverBase::signalEvent;

public:
	void init() {
#if defined(PN5180_ENABLE_LPCD) || 1
		esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
		if (wakeupReason == ESP_SLEEP_WAKEUP_EXT1)
		{
			wakeupCheck();
		}
		// we are still here, so disable deep sleep
		gpio_deep_sleep_hold_dis();
		gpio_hold_dis(gpio_num_t(RFID_CS));
		gpio_hold_dis(gpio_num_t(RFID_RST));
#if (RFID_IRQ >= 0 && RFID_IRQ <= MAX_GPIO)
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
	}

	void wakeupCheck() {
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
		PN5180ISO14443 nfc14443(RFID_CS, RFID_BUSY, RFID_RST);
		PN5180ISO15693 nfc15693(RFID_CS, RFID_BUSY, RFID_RST);
		RfidPN1580 *driver = static_cast<RfidPN1580 *>(ptr);
		MainFsm fsm = MainFsm::init;
		Nfc14443Fsm nfc14443Fsm = Nfc14443Fsm::reset;
		Nfc15693Fsm nfc15693Fsm = Nfc15693Fsm::reset;
		uint32_t lastTimeCardDetect = 0;
		uint8_t uid[10];
		uint8_t lastCardId[Message::cardIdSize];
		bool cardAppliedLastRun = false;

		while (1) {
			bool cardReceived = false;

			vTaskDelay(portTICK_RATE_MS * 10u);

			switch (fsm) {
				case MainFsm::init:
				{
					nfc14443.begin();
					nfc14443.reset();
					// show PN1580 reader version
					uint8_t firmware[2];
					nfc14443.readEEprom(FIRMWARE_VERSION, firmware, sizeof(firmware));
					Log_Printf(LOGLEVEL_DEBUG, "PN5180 firmware version=%d.%d", firmware[1], firmware[0]);

					// activate RF field
					delay(4);
					Log_Println(rfidScannerReady, LOGLEVEL_DEBUG);

					// start with NFC1443 detection
					fsm = MainFsm::nfc14443;
				}
				break;

				case MainFsm::nfc14443:
					switch (nfc14443Fsm) {
					case Nfc14443Fsm::reset:
						nfc14443.reset();
						nfc14443Fsm = Nfc14443Fsm::readcard;
						break;

					case Nfc14443Fsm::readcard:
						if (nfc14443.readCardSerial(uid) >= 4) {
							cardReceived = true;
							lastTimeCardDetect = millis();
						} else {
							// Reset to dummy-value if no card is there
							// Necessary to differentiate between "card is still applied" and "card is re-applied again after removal"
							// lastTimeCardDetect is used to prevent "new card detection with old card" with single events where no card was detected
							if (!lastTimeCardDetect || (millis() - lastTimeCardDetect) >= RfidPN1580::cardDetectTimeout) {
								lastTimeCardDetect = 0;
								memset(lastCardId, 0, sizeof(lastCardId));

								// try nfc15693
								nfc14443Fsm = Nfc14443Fsm::reset;
								fsm = MainFsm::nfc15693;
							}
						}
						break;
					}
					break;

			case MainFsm::nfc15693:
				switch (nfc15693Fsm) {
					case Nfc15693Fsm::reset:
						nfc15693.reset();
						nfc15693.setupRF();

						nfc15693Fsm = Nfc15693Fsm::readcard;
						break;

					case Nfc15693Fsm::disablePrivacyMode:
					{
							// we are in privacy mode, try to unlock first
							constexpr size_t rfidPwdCount = sizeof(rfidPassword) / sizeof(rfidPassword[0]);

							// try all passwords until one works
							for(size_t i=0;i<rfidPwdCount;i++) {
								uint8_t pwd[4];
								memcpy(pwd, rfidPassword[i], 4);

								ISO15693ErrorCode ret = nfc15693.disablePrivacyMode(pwd);
								if(ret == ISO15693_EC_OK) {
									Log_Printf(LOGLEVEL_NOTICE, rfid15693TagUnlocked, i);
									nfc15693Fsm = Nfc15693Fsm::readcard;
									break;
								}
							}
							// none of our password worked
							Log_Println(rfid15693TagUnlockFailed, LOGLEVEL_ERROR);

							nfc15693Fsm = Nfc15693Fsm::reset;
					}
					break;

					case Nfc15693Fsm::readcard:
					{
						// try to read ISO15693 inventory
						ISO15693ErrorCode ret = nfc15693.getInventory(uid);
						if (ret == ISO15693_EC_OK) {
							cardReceived = true;
							lastTimeCardDetect = millis();
						} else if(ret == ISO15693_EC_BLOCK_IS_LOCKED) {
							// we have a locked chip, try to unlock
							nfc15693Fsm = Nfc15693Fsm::disablePrivacyMode;
						} else {
							// lastTimeDetected15693 is used to prevent "new card detection with old card" with single events where no card was detected
							if (!lastTimeCardDetect || (millis() - lastTimeCardDetect >= 400)) {
								lastTimeCardDetect = 0;
								memset(lastCardId, 0, sizeof(lastCardId));

								// try nfc14443 next
								nfc15693Fsm = Nfc15693Fsm::reset;
								fsm = MainFsm::nfc14443;
							}
						}
					}
					break;
				}
				break;
			}
	
			if(cardReceived) {
				// check if it is the same card
				if(memcmp(uid, lastCardId, Message::cardIdSize) == 0) {
					// same card, reset reader
					nfc14443Fsm = Nfc14443Fsm::reset;
					nfc15693Fsm = Nfc15693Fsm::reset;
				}
				continue;

				memcpy(lastCardId, uid, Message::cardIdSize);

				Message msg;
				msg.event = Message::Event::CardApplied;
				memcpy(msg.cardId, uid,  Message::cardIdSize);

				Log_Printf(LOGLEVEL_NOTICE, rfidTagDetected, msg.toHexString().c_str());
				Log_Printf(LOGLEVEL_NOTICE, "Card type: %s", (fsm == MainFsm::nfc14443) ? "ISO-14443" : "ISO-15693");

				driver->signalEvent(Message::Event::CardApplied, uid);
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

	TaskHandle_t taskHandle{nullptr};
	bool enableLpcdShutdown{false};
};

} // namespace implementation

using RfidDriver = implementation::RfidPN1580;

} // namespace rfid::driver
