#pragma once

#include "settings.h"

#include "RfidEvent.h"
#include "crtp.hpp"

#include <WString.h>
#include <array>
#include <mutex>

namespace rfid::driver {

template <typename Derived>
class RfidDriverBase : crtp<Derived, RfidDriverBase> {
public:
	void init() {
		this->underlying().init();
	}

	const Message &getLastEvent() {
		std::lock_guard guard(accessGuard);
		return message;
	}

	void suspend(bool enable) {
		this->underlying().suspend(enable);
	}

	void wakeupCheck() {
		this->underlying().wakeupCheck();
	}

	bool waitForCardEvent(uint32_t ms) {
		return xSemaphoreTake(cardChangeEvent, ms / portTICK_PERIOD_MS);
	}

	void exit() {
		this->underlying().exit();
	}

	void signalEvent(const Message::Event event, const CardIdType &cardId = {}) {
		// get the access guard
		std::lock_guard guard(accessGuard);
		message.event = event;
		message.cardId = cardId;

		// signal the event
		xSemaphoreGive(cardChangeEvent);
	}

	void signalEvent(const Message &msg) {
		signalEvent(msg.event, msg.cardId);
	}

protected:
	SemaphoreHandle_t cardChangeEvent {xSemaphoreCreateBinary()};
	std::mutex accessGuard;
	Message message;

private:
	DEFINE_HAS_SIGNATURE(hasInit, T::init, void (T::*)(void));
	DEFINE_HAS_SIGNATURE(hasSuspend, T::suspend, void (T::*)(bool));
	DEFINE_HAS_SIGNATURE(hasWakeupCheck, T::wakeupCheck, void (T::*)(void));
	DEFINE_HAS_SIGNATURE(hasExit, T::exit, void (T::*)(void));

	RfidDriverBase() {
		static_assert(hasInit<Derived>::value, "Derived class must implement init");
		static_assert(hasSuspend<Derived>::value, "Derived class must implement suspend");
		static_assert(hasWakeupCheck<Derived>::value, "Derived class must implement wakeupCheck");
		static_assert(hasExit<Derived>::value, "Derived class must implement exit");
	}
	friend Derived;
};

} // namespace rfid::driver

// select a driver driver here, the driver is then instanciated with "RfidDriver rfid;" in Rfid.cpp
#if defined(RFID_READER_TYPE_PN5180)
	#include "pn5180.hpp"
#elif defined(RFID_READER_TYPE_MFRC522)
	#include "mfrc522.hpp"
#elif defined(RFID_READER_TYPE_PN532)
	#include "pn532.hpp"
#else
	#warning No RFID card presendriver selected
	#undef RFID_READER_ENABLED
#endif
