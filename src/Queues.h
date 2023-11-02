#pragma once

extern QueueHandle_t gVolumeQueue;
extern QueueHandle_t gTrackControlQueue;

void Queues_Init(void);

#include "Rfid.h"

#include <memory>
#include <mutex>

template <typename T>
class SharedObject {
public:
	SharedObject() = default;
	~SharedObject() = default;

	const T &get() {
		std::lock_guard guard(mutex);
		return obj;
	}

	void put(T newObj) {
		std::lock_guard guard(mutex);
		obj = newObj;
	}

private:
	T obj {};
	std::mutex mutex {};
};

extern SharedObject<int> gVolume;
extern SharedObject<uint8_t> gTrackControl;
extern SharedObject<char[cardIdStringSize]> gRfidCard;
