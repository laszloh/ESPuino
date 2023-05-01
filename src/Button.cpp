#include <Arduino.h>
#include "settings.h"

#include "Button.h"

#include "Cmd.h"
#include "Log.h"
#include "Port.h"
#include "System.h"
#include "cpp.h"

bool gButtonInitComplete = false;

// Only enable those buttons that are not disabled (99 or >115)
// 0 -> 39: GPIOs
// 100 -> 115: Port-expander
auto gButtons = std::to_array<t_button>({
	{BUTTON_0, BUTTON_0_ACTIVE_STATE, BUTTON_0_SHORT, BUTTON_0_LONG},
	{BUTTON_1, BUTTON_1_ACTIVE_STATE, BUTTON_1_SHORT, BUTTON_1_LONG},
	{BUTTON_2, BUTTON_2_ACTIVE_STATE, BUTTON_2_SHORT, BUTTON_2_LONG},
	{BUTTON_3, BUTTON_3_ACTIVE_STATE, BUTTON_3_SHORT, BUTTON_3_LONG},
	{BUTTON_4, BUTTON_4_ACTIVE_STATE, BUTTON_4_SHORT, BUTTON_4_LONG},
	{BUTTON_5, BUTTON_5_ACTIVE_STATE, BUTTON_5_SHORT, BUTTON_5_LONG},
});

struct MultiButtonAction {
	uint8_t btn1{99};
	uint8_t btn2{99};
	uint8_t cmd{CMD_NOTHING};

	constexpr MultiButtonAction(uint8_t btn1, uint8_t btn2, uint8_t cmd) 
		: btn1(btn1), btn2(btn2), cmd(cmd) { }
	constexpr MultiButtonAction() {}
};

/**
 * @brief Creates the array of MultiButtonActions of all combinations which are enabled
 * 
 * This function purges all multi button combinations with CMD_NOTHING and returns the remaining active commands as a constexpr std::array of MultiButtonActions.
 * @return constexpr std::array<MultiButtonAction, [numActions]>
 */
constexpr auto createMultiButtonArray() {
	struct MultiButtonHelper {
		uint8_t btn1, btn2, cmd;
		constexpr MultiButtonHelper(uint8_t btn1, uint8_t btn2, uint8_t cmd)
			: btn1(btn1), btn2(btn2), cmd(cmd) { }
	};

	// this is needed since we need to know all the button combination from the settings file
	constexpr auto buttonToArray = std::to_array<MultiButtonHelper>({
		// Button 0 combies
		{0, 1, BUTTON_MULTI_01},
		{0, 2, BUTTON_MULTI_02},
		{0, 3, BUTTON_MULTI_03},
		{0, 4, BUTTON_MULTI_04},
		{0, 5, BUTTON_MULTI_05},

		// Button 1 combies
		{1, 2, BUTTON_MULTI_12},
		{1, 3, BUTTON_MULTI_13},
		{1, 4, BUTTON_MULTI_14},
		{1, 5, BUTTON_MULTI_15},

		// Button 2 combies
		{2, 3, BUTTON_MULTI_23},
		{2, 4, BUTTON_MULTI_24},
		{2, 5, BUTTON_MULTI_25},

		// Button 3 combies
		{3, 4, BUTTON_MULTI_34},
		{3, 5, BUTTON_MULTI_35},

		// Button 4 combies
		{4, 5, BUTTON_MULTI_45},
	});

	// this lambda calculates the final size of the command array, we are only interested in commands != CMD_NOTHING
	constexpr auto numMultiEvents = [buttonToArray](){
		size_t count = 0;
		for(const auto e : buttonToArray) {
			if(e.cmd != CMD_NOTHING) {
				count++;
			}
		}

		return count;
	};

	// create the return array...
	std::array<MultiButtonAction, numMultiEvents()> btnActionArray{};
	size_t idx = 0;

	// and populate it with all combinations with cmd != CMD_NOTHING
	for(const auto e : buttonToArray) {
		if(e.cmd != CMD_NOTHING) {
			// add element to array
			btnActionArray[idx] = MultiButtonAction(e.btn1, e.btn2, e.cmd);
			idx++;
		}
	}

	// and return it
	return btnActionArray;
}

constexpr auto multiBtnActions = createMultiButtonArray();	// The object holding all registered multi button commands

uint8_t gShutdownButton = 99; // Helper used for Neopixel: stores button-number of shutdown-button
uint16_t gLongPressTime = 0;

#ifdef PORT_EXPANDER_ENABLE
extern bool Port_AllowReadFromPortExpander;
#endif

static volatile SemaphoreHandle_t Button_TimerSemaphore;

hw_timer_t *Button_Timer = NULL;
static void IRAM_ATTR onTimer();
static void Button_DoButtonActions(void);

void Button_Init() {
	// process all buttons
	uint8_t idx = 0;
	for(auto it = gButtons.begin(); it != gButtons.end(); it++, idx++) {
		if(!it->enabled) {
			continue;
		}

		if(it->internal) {
			const auto mode = (it->inverted) ? INPUT_PULLDOWN : INPUT_PULLUP;
			pinMode(it->gpio, mode);
			if(it->wakeButton) {
				// register wakeup
				if(esp_sleep_enable_ext0_wakeup((gpio_num_t)it->gpio, (it->inverted) ? 1 : 0) == ESP_ERR_INVALID_ARG) {
					Log_Printf(LOGLEVEL_ERROR, wrongWakeUpGpio, WAKEUP_BUTTON);
				}
			}
		}

		if(it->cmdLong == CMD_SLEEPMODE) {
			gShutdownButton = idx;
		}
	}

	// Create 1000Hz-HW-Timer (currently only used for buttons)
	Button_TimerSemaphore = xSemaphoreCreateBinary();
	Button_Timer = timerBegin(0, 240, true); // Prescaler: CPU-clock in MHz
	timerAttachInterrupt(Button_Timer, &onTimer, true);
	timerAlarmWrite(Button_Timer, 10000, true); // 100 Hz
	timerAlarmEnable(Button_Timer);
}

// If timer-semaphore is set, read buttons (unless controls are locked)
void Button_Cyclic() {
	if (xSemaphoreTake(Button_TimerSemaphore, 0) == pdTRUE) {
		unsigned long currentTimestamp = millis();
#ifdef PORT_EXPANDER_ENABLE
		Port_Cyclic();
#endif

		if (System_AreControlsLocked()) {
			return;
		}

		// Iterate over all buttons in struct-array
		for(auto &e : gButtons) {
			if(!e.enabled) {
				continue;
			}

			// Buttons can be mixed between GPIO and port-expander.
			// But at the same time only one of them can be for example BUTTON_0
			e.currentState = Port_Read(e.gpio) ^ e.inverted;

			if (e.currentState != e.lastState && currentTimestamp - e.lastPressedTimestamp > buttonDebounceInterval) {
				if (!e.currentState) {
					e.isPressed = true;
					e.lastPressedTimestamp = currentTimestamp;
					if(!e.firstPressedTimestamp) {
						e.firstPressedTimestamp = currentTimestamp;
					}
				} else {
					e.isReleased = true;
					e.lastReleasedTimestamp = currentTimestamp;
					e.firstPressedTimestamp = 0;
				}
			}
			e.lastState = e.currentState;
		}
	}
	gButtonInitComplete = true;
	Button_DoButtonActions();
}

// Do corresponding actions for all buttons
void Button_DoButtonActions(void) {

	// check all registered multi buttons for an action
	for(auto &mb : multiBtnActions) {
		if(gButtons[mb.btn1].isPressed && gButtons[mb.btn2].isPressed) {
			gButtons[mb.btn1].isPressed = false;
			gButtons[mb.btn2].isPressed = false;
			Cmd_Action(mb.cmd);
			return;
		}
	}

	// there was no multi button action, check all single button actions
	for(auto &e : gButtons) {
		if(e.isPressed) {
			if(e.lastReleasedTimestamp > e.lastPressedTimestamp) {
				if(e.lastReleasedTimestamp - e.lastPressedTimestamp < intervalToLongPress) {
					Cmd_Action(e.cmdShort);
				} else {
					// if not volume buttons than start action after button release
					if (e.cmdLong != CMD_VOLUMEUP && e.cmdLong != CMD_VOLUMEDOWN) {
						Cmd_Action(e.cmdLong);
					}
				}
				e.isPressed = false;
			} else if (e.cmdLong == CMD_VOLUMEUP && e.cmdLong == CMD_VOLUMEDOWN) {
				unsigned long currentTimestamp = millis();
				if(currentTimestamp - e.lastPressedTimestamp > intervalToLongPress) {
					// calculate remainder
					uint32_t remainder = (currentTimestamp - e.lastPressedTimestamp) % intervalToLongPress;

					// trigger action if remainder rolled over
					if(remainder < gLongPressTime) {
						Cmd_Action(e.cmdLong);
					}

					gLongPressTime = remainder;
				}
			}
		}
	}
}

void IRAM_ATTR onTimer() {
	xSemaphoreGiveFromISR(Button_TimerSemaphore, NULL);
}
