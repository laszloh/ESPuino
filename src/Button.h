#pragma once

#include <Arduino.h>
#include <array>
#include "settings.h"

namespace button
{

class Button {
public:
	Button(uint8_t gpio, uint8_t activeState, uint8_t cmdShort, uint8_t cmdLong) 
		: gpio(gpio), cmdShort(cmdShort), cmdLong(cmdLong),
		  enabled(gpio != 99), inverted(activeState == 1), internal(gpio < GPIO_NUM_MAX), 
		  wakeButton(gpio == WAKEUP_BUTTON) {
		lastState = currentState = isPressed = isReleased = false;
		  }

protected:
	const uint8_t gpio;
	const uint8_t cmdShort;
	const uint8_t cmdLong;
	const bool enabled : 1;
	const bool inverted : 1;
	const bool internal : 1;
	const bool wakeButton : 1;

	bool lastState : 1;
	bool currentState:1;
	bool isPressed : 1;
	bool isReleased : 1;
	unsigned long lastPressedTimestamp{0};
	unsigned long lastReleasedTimestamp{0};
	unsigned long firstPressedTimestamp{0};
};

struct t_button {
	const uint8_t gpio;
	const uint8_t cmdShort;
	const uint8_t cmdLong;
	const bool enabled : 1;
	const bool inverted : 1;
	const bool internal : 1;
	const bool wakeButton : 1;

	bool lastState : 1;
	bool currentState:1;
	bool isPressed : 1;
	bool isReleased : 1;
	unsigned long lastPressedTimestamp{0};
	unsigned long lastReleasedTimestamp{0};
	unsigned long firstPressedTimestamp{0};

	t_button(uint8_t gpio, uint8_t activeState, uint8_t cmdShort, uint8_t cmdLong) 
		: gpio(gpio), cmdShort(cmdShort), cmdLong(cmdLong),
		  enabled(gpio != 99), inverted(activeState == 1), internal(gpio < GPIO_NUM_MAX), 
		  wakeButton(gpio == WAKEUP_BUTTON) {
		lastState = currentState = isPressed = isReleased = false;
	}
};

std::optional<const t_button> getShutdownButton();
bool isInitComplete();

void init(void);
void cyclic(void);


} // namespace button
