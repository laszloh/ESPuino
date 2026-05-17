#pragma once

#include "driver/gpio.h"

#define GPIO_IS_VALID_EXPANDER_GPIO(gpio_num) (((gpio_num) >= 100) && ((gpio_num) <= 115))

struct GpioPin {
	const uint8_t index {0};
	const uint8_t pinNumber {99};
	const bool activeState {false};
	const bool expanderPin {false};
	const bool wakeUpPin {false};

	uint8_t shortPressCmd {CMD_NOTHING};
	uint8_t longPressCmd {CMD_NOTHING};
	bool lastState {false};
	bool currentState {false};
	bool isPressed {false};
	bool isReleased {false};
	unsigned long lastPressedTimestamp {0};
	unsigned long lastReleasedTimestamp {0};
	unsigned long firstPressedTimestamp {0};

	constexpr GpioPin() = default;
	constexpr GpioPin(uint8_t idx, uint8_t pinNumber, bool activeState, uint8_t shortPressCmd, uint8_t longPressCmd)
		: index(idx)
		, pinNumber(pinNumber)
		, activeState(activeState)
		, expanderPin(GPIO_IS_VALID_EXPANDER_GPIO(pinNumber))
		, wakeUpPin(pinNumber == WAKEUP_BUTTON)
		, shortPressCmd(shortPressCmd)
		, longPressCmd(longPressCmd) { }

	constexpr bool isActive() const {
		if (expanderPin) {
			return true;
		}
		return GPIO_IS_VALID_GPIO(static_cast<gpio_num_t>(pinNumber));
	}
};

extern GpioPin buttons[];
extern uint8_t gShutdownButton;
extern bool gButtonInitComplete;

void Button_Init(void);
void Button_Cyclic(void);
void Button_LoadConfig();
