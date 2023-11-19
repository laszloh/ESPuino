#pragma once

#include "Log.h"

#include <WString.h>
#include <array>
#include <stdint.h>

namespace rfid {

constexpr size_t cardIdSize = 4;

class CardIdType : public std::array<uint8_t, cardIdSize> {
public:
	void assign(const uint8_t *arr, const size_t arrSize = cardIdSize) {
		std::copy(arr, arr + std::min(arrSize, size()), begin());
	}

	void assign(const char *str) {
		if (strlen(str) < 3 * size()) {
			Log_Printf(LOGLEVEL_ERROR, "Received string if too shoirt: %s\n", str);
			return;
		}

		// convert the string to binary
		for (int i = 0; i < size(); i++) {
			char tmp[4] = {0};
			memcpy(tmp, str + 3 * i, 3);
			
			operator[](i) = atoi(tmp);
		}
	}

	explicit operator bool() const {
		return !std::all_of(begin(), end(), [](bool e) { return e == 0; });
	}

	const String toDezimalString() const {
		char buf[3 * cardIdSize + 1] = {0};

		for (int i = 0; i < cardIdSize; i++) {
			const size_t idx = i * 3;
			buf[idx] = '0' + operator[](i) / 100;
			buf[idx + 1] = '0' + (operator[](i) % 100) / 10;
			buf[idx + 2] = '0' + (operator[](i) % 100) % 10;
		}

		return buf;
	}

	const String toHexString() const {
		char buf[3 * cardIdSize] = {0};

		constexpr char hexDigits[] = "0123456789ABCDEF";
		for (size_t i = 0; i < cardIdSize; i++) {
			const size_t idx = i * 3;
			buf[idx] = hexDigits[(operator[](i) >> 4) & 0x0F];
			buf[idx + 1] = hexDigits[operator[](i) & 0x0F];
			buf[idx + 2] = (i < cardIdSize - 1) ? '-' : '\0';
		}

		return buf;
	}
};

struct Message {
	enum class Event : uint8_t {
		NoCard = 0,
		CardRemoved,
		CardApplied,
		CardPresent
	};

	Event event;
	CardIdType cardId;

	inline bool operator==(const Message &rhs) {
		return (event == rhs.event) && (cardId == rhs.cardId);
	}
	inline bool operator!=(const Message &rhs) { return !operator==(rhs); }
};

} // namespace rfid
