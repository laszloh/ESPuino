#include <Arduino.h>
#include "settings.h"

#include "Button.h"

#include "Cmd.h"
#include "Log.h"
#include "Port.h"
#include "System.h"

#include <FastLED.h>
#include <algorithm>
#include <array>
#include <iterator>

bool gButtonInitComplete = false;
uint16_t gLongPressTime = 0;

#ifdef PORT_EXPANDER_ENABLE
extern bool Port_AllowReadFromPortExpander;
#endif

static GpioPin buttons[] = {
	GpioPin {0, BUTTON_0, BUTTON_0_ACTIVE_STATE, BUTTON_0_SHORT, BUTTON_0_LONG},
	GpioPin {1, BUTTON_1, BUTTON_1_ACTIVE_STATE, BUTTON_1_SHORT, BUTTON_1_LONG},
	GpioPin {2, BUTTON_2, BUTTON_2_ACTIVE_STATE, BUTTON_2_SHORT, BUTTON_2_LONG},
	GpioPin {3, BUTTON_3, BUTTON_3_ACTIVE_STATE, BUTTON_3_SHORT, BUTTON_3_LONG},
	GpioPin {4, BUTTON_4, BUTTON_4_ACTIVE_STATE, BUTTON_4_SHORT, BUTTON_4_LONG},
	GpioPin {5, BUTTON_5, BUTTON_5_ACTIVE_STATE, BUTTON_5_SHORT, BUTTON_5_LONG}
};

struct MultiButtonAction {
	uint8_t btn1 {99};
	uint8_t btn2 {99};
	const char *cfg {nullptr};
	uint8_t cmd {CMD_NOTHING};

	constexpr MultiButtonAction(uint8_t btn1, uint8_t btn2, const char *cfg, uint8_t cmd)
		: btn1(btn1)
		, btn2(btn2)
		, cfg(cfg)
		, cmd(cmd) { }
	constexpr MultiButtonAction() { }
};

/**
 * @brief Creates the array of MultiButtonActions of all combinations which are enabled
 *
 * This function purges all multi button combinations with CMD_NOTHING and returns the remaining active commands as a constexpr std::array of MultiButtonActions.
 * @return constexpr std::array<MultiButtonAction, [numActions]>
 */
consteval auto createMultiButtonArray() {
	// this is needed since we need to know all the button combination from the settings file
	constexpr MultiButtonAction buttonToArray[] = {
		// Button 0 combies
		{0, 1, "btnMulti01", BUTTON_MULTI_01},
		{0, 2, "btnMulti02", BUTTON_MULTI_02},
		{0, 3, "btnMulti03", BUTTON_MULTI_03},
		{0, 4, "btnMulti04", BUTTON_MULTI_04},
		{0, 5, "btnMulti05", BUTTON_MULTI_05},

		// Button 1 combies
		{1, 2, "btnMulti12", BUTTON_MULTI_12},
		{1, 3, "btnMulti13", BUTTON_MULTI_13},
		{1, 4, "btnMulti14", BUTTON_MULTI_14},
		{1, 5, "btnMulti15", BUTTON_MULTI_15},

		// Button 2 combies
		{2, 3, "btnMulti23", BUTTON_MULTI_23},
		{2, 4, "btnMulti24", BUTTON_MULTI_24},
		{2, 5, "btnMulti25", BUTTON_MULTI_25},

		// Button 3 combies
		{3, 4, "btnMulti34", BUTTON_MULTI_34},
		{3, 5, "btnMulti35", BUTTON_MULTI_35},

		// Button 4 combies
		{4, 5, "btnMulti45", BUTTON_MULTI_45},
	};

	const auto isActive = [](uint8_t pin) {
		if (GPIO_IS_VALID_EXPANDER_GPIO(pin)) {
			return true; // port-expander pin
		}
		return GPIO_IS_VALID_GPIO(static_cast<gpio_num_t>(pin));
	};

	constexpr uint8_t buttonPins[] = {BUTTON_0, BUTTON_1, BUTTON_2, BUTTON_3, BUTTON_4, BUTTON_5};

	// this lambda calculates the final size of the command array, we are only interested in combos whose buttons are both wired up
	constexpr size_t numMultiEvents = std::count_if(std::begin(buttonToArray), std::end(buttonToArray), [&isActive, &buttonPins](const MultiButtonAction &e) {
		return isActive(buttonPins[e.btn1]) && isActive(buttonPins[e.btn2]);
	});

	// create the return array...
	std::array<MultiButtonAction, numMultiEvents> btnActionArray {};
	size_t idx = 0;

	// and populate it with all combinations with cmd != CMD_NOTHING
	for (const auto &e : buttonToArray) {
		if (isActive(buttonPins[e.btn1]) && isActive(buttonPins[e.btn2])) {
			// add element to array
			btnActionArray[idx] = e;
			idx++;
		}
	}

	// and return it
	return btnActionArray;
}
auto multiButtonCombos = createMultiButtonArray();

static void Button_UpdateState(GpioPin &btn, unsigned long currentTimestamp);
static void Button_DoButtonActions(void);

const GpioPin *Button_GetShutdownButton() {
	auto it = std::find_if(std::begin(buttons), std::end(buttons),
		[](const GpioPin &b) { return b.longPressCmd == CMD_SLEEPMODE; });
	return (it != std::end(buttons)) ? &*it : nullptr;
}

void Button_Init() {
	for (const auto &btn : buttons) {
		if (!btn.expanderPin) {
			if (btn.activeState) {
				pinMode(btn.pinNumber, INPUT);
			} else {
				pinMode(btn.pinNumber, INPUT_PULLUP);
			}
		}
		if (btn.wakeUpPin) {
			if (ESP_ERR_INVALID_ARG == esp_sleep_enable_ext0_wakeup((gpio_num_t) btn.pinNumber, 0)) {
				Log_Printf(LOGLEVEL_ERROR, wrongWakeUpGpio, btn.pinNumber);
			}
		}
	}
	// load short- and long-press commands from NVS
	Button_LoadConfig();
	gButtonInitComplete = true;
}

void Button_LoadConfig() {
	char cmdShortKey[10] = "btnShort0";
	char cmdLongKey[10] = "btnLong0";

	for (auto &e : buttons) {
		cmdShortKey[8] = '0' + e.index;
		cmdLongKey[7] = '0' + e.index;
		e.shortPressCmd = gPrefsSettings.getUChar(cmdShortKey, e.shortPressCmd);
		e.longPressCmd = gPrefsSettings.getUChar(cmdLongKey, e.longPressCmd);
	}
}

void Button_Cyclic() {
	static CEveryNMillis buttonCycleTimer(10); // check buttons every 10ms

	if (buttonCycleTimer) {
		unsigned long currentTimestamp = millis();

#ifdef PORT_EXPANDER_ENABLE
		Port_Cyclic();
#endif

		if (System_AreControlsLocked()) {
			return;
		}

		for (auto &btn : buttons) {
			btn.currentState = Port_Read(btn.pinNumber) ^ btn.activeState;
			Button_UpdateState(btn, currentTimestamp);
		}

		Button_DoButtonActions();
	}
}

// Update press/release state for a single button with debouncing
static void Button_UpdateState(GpioPin &btn, unsigned long currentTimestamp) {
	const bool stateChanged = btn.currentState != btn.lastState;
	const bool debounceElapsed = currentTimestamp - btn.lastPressedTimestamp > buttonDebounceInterval;

	if (stateChanged && debounceElapsed) {
		const bool buttonPressed = !btn.currentState;
		if (buttonPressed) {
			btn.isPressed = true;
			btn.lastPressedTimestamp = currentTimestamp;
			if (!btn.firstPressedTimestamp) {
				btn.firstPressedTimestamp = currentTimestamp;
			}
		} else {
			btn.isReleased = true;
			btn.lastReleasedTimestamp = currentTimestamp;
			btn.firstPressedTimestamp = 0;
		}
	}
	btn.lastState = btn.currentState;
}

// Check for multi-button combinations and execute corresponding action
static bool Button_HandleMultiButtonPress(void) {
	for (const auto &combo : multiButtonCombos) {
		if (buttons[combo.btn1].isPressed && buttons[combo.btn2].isPressed) {
			buttons[combo.btn1].isPressed = false;
			buttons[combo.btn2].isPressed = false;
			Cmd_Action(combo.cmd);
			return true;
		}
	}
	return false;
}

// Handle a single button's short/long press action
static void Button_HandleSinglePress(GpioPin &btn, unsigned long currentTimestamp) {
	const unsigned long pressDuration = currentTimestamp - btn.lastPressedTimestamp;
	const bool wasReleased = btn.lastReleasedTimestamp > btn.lastPressedTimestamp;

	// Handle button release (short or long press completed)
	if (wasReleased) {
		const unsigned long releaseDuration = btn.lastReleasedTimestamp - btn.lastPressedTimestamp;
		const bool wasShortPress = releaseDuration < intervalToLongPress;

		if (wasShortPress) {
			Cmd_Action(btn.shortPressCmd);
		} else if (btn.longPressCmd == CMD_SLEEPMODE) {
			// Sleep-mode only triggers on release to prevent immediate wake-up
			Cmd_Action(btn.longPressCmd);
		}

		btn.isPressed = false;
		return;
	}

	// Handle volume buttons with repeat functionality
	if (btn.longPressCmd == CMD_VOLUMEUP || btn.longPressCmd == CMD_VOLUMEDOWN) {
		if (pressDuration <= intervalToLongPress) {
			return;
		}
		uint16_t remainder = pressDuration % intervalToLongPress;
		if (remainder < gLongPressTime) {
			Cmd_Action(btn.longPressCmd);
		}
		gLongPressTime = remainder;
		return;
	}

	// Handle other long-press actions (except sleep mode which triggers on release)
	if (btn.longPressCmd != CMD_SLEEPMODE && pressDuration > intervalToLongPress) {
		btn.isPressed = false;
		Cmd_Action(btn.longPressCmd);
	}
}

// Do corresponding actions for all buttons
static void Button_DoButtonActions() {
	if (Button_HandleMultiButtonPress()) {
		return;
	}

	unsigned long currentTimestamp = millis();
	for (auto &btn : buttons) {
		if (btn.isPressed) {
			Button_HandleSinglePress(btn, currentTimestamp);
		}
	}
}
