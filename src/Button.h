#pragma once

#include <Arduino.h>
#include <array>
#include "settings.h"

namespace button
{

struct t_button {
	const uint8_t gpio;
	const uint8_t cmdShort;
	const uint8_t cmdLong;
	const ButtonLongTrigger longTrigger;
	const bool enabled;
	const bool inverted;
	const bool internal;
	const bool wakeButton;

	bool lastState {false};
	bool isPressed {false};
	uint32_t lastPressedTimestamp{0};
	uint32_t lastReleasedTimestamp{0};
	uint32_t longPressRemainder{0};

	constexpr t_button(uint8_t gpio, uint8_t activeState, uint8_t cmdShort, uint8_t cmdLong, ButtonLongTrigger longTrigger)
		: gpio(gpio), cmdShort(cmdShort), cmdLong(cmdLong), longTrigger(longTrigger),
		  enabled(gpio != 99 && (cmdShort != CMD_NOTHING || cmdLong!= CMD_NOTHING)), inverted(activeState == 1), internal(gpio < GPIO_NUM_MAX), 
		  wakeButton(gpio == WAKEUP_BUTTON) {
	}
};

std::optional<const t_button> getShutdownButton();
bool isInitComplete();

void init(void);
void cyclic(void);


} // namespace button
