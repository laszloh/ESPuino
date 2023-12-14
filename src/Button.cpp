#include <Arduino.h>
#include "settings.h"

#include "Button.h"

#include "Cmd.h"
#include "Log.h"
#include "Port.h"
#include "System.h"

bool gButtonInitComplete = false;

// Only enable those buttons that are not disabled (99 or >115)
// 0 -> 39: GPIOs
// 100 -> 115: Port-expander
#if (BUTTON_0 >= 0 && BUTTON_0 <= MAX_GPIO)
	#define BUTTON_0_ENABLE
#elif (BUTTON_0 >= 100 && BUTTON_0 <= 115)
	#define EXPANDER_0_ENABLE
#endif
#if (BUTTON_1 >= 0 && BUTTON_1 <= MAX_GPIO)
	#define BUTTON_1_ENABLE
#elif (BUTTON_1 >= 100 && BUTTON_1 <= 115)
	#define EXPANDER_1_ENABLE
#endif
#if (BUTTON_2 >= 0 && BUTTON_2 <= MAX_GPIO)
	#define BUTTON_2_ENABLE
#elif (BUTTON_2 >= 100 && BUTTON_2 <= 115)
	#define EXPANDER_2_ENABLE
#endif
#if (BUTTON_3 >= 0 && BUTTON_3 <= MAX_GPIO)
	#define BUTTON_3_ENABLE
#elif (BUTTON_3 >= 100 && BUTTON_3 <= 115)
	#define EXPANDER_3_ENABLE
#endif
#if (BUTTON_4 >= 0 && BUTTON_4 <= MAX_GPIO)
	#define BUTTON_4_ENABLE
#elif (BUTTON_4 >= 100 && BUTTON_4 <= 115)
	#define EXPANDER_4_ENABLE
#endif
#if (BUTTON_5 >= 0 && BUTTON_5 <= MAX_GPIO)
	#define BUTTON_5_ENABLE
#elif (BUTTON_5 >= 100 && BUTTON_5 <= 115)
	#define EXPANDER_5_ENABLE
#endif

t_button gButtons[7]; // next + prev + pplay + rotEnc + button4 + button5 + dummy-button
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
#if (WAKEUP_BUTTON >= 0 && WAKEUP_BUTTON <= MAX_GPIO)
	if (ESP_ERR_INVALID_ARG == esp_sleep_enable_ext0_wakeup((gpio_num_t) WAKEUP_BUTTON, 0)) {
		Log_Printf(LOGLEVEL_ERROR, wrongWakeUpGpio, WAKEUP_BUTTON);
	}
#endif

#ifdef NEOPIXEL_ENABLE // Try to find button that is used for shutdown via longpress-action (only necessary for Neopixel)
	#if defined(BUTTON_0_ENABLE) || defined(EXPANDER_0_ENABLE)
		#if (BUTTON_0_LONG == CMD_SLEEPMODE)
	gShutdownButton = 0;
		#endif
	#endif
	#if defined(BUTTON_1_ENABLE) || defined(EXPANDER_1_ENABLE)
		#if (BUTTON_1_LONG == CMD_SLEEPMODE)
	gShutdownButton = 1;
		#endif
	#endif
	#if defined(BUTTON_2_ENABLE) || defined(EXPANDER_2_ENABLE)
		#if (BUTTON_2_LONG == CMD_SLEEPMODE)
	gShutdownButton = 2;
		#endif
	#endif
	#if defined(BUTTON_3_ENABLE) || defined(EXPANDER_3_ENABLE)
		#if (BUTTON_3_LONG == CMD_SLEEPMODE)
	gShutdownButton = 3;
		#endif
	#endif
	#if defined(BUTTON_4_ENABLE) || defined(EXPANDER_4_ENABLE)
		#if (BUTTON_4_LONG == CMD_SLEEPMODE)
	gShutdownButton = 4;
		#endif
	#endif
	#if defined(BUTTON_5_ENABLE) || defined(EXPANDER_5_ENABLE)
		#if (BUTTON_5_LONG == CMD_SLEEPMODE)
	gShutdownButton = 5;
		#endif
	#endif
#endif

// Activate internal pullups for all enabled buttons connected to GPIOs
#ifdef BUTTON_0_ENABLE
	if (BUTTON_0_ACTIVE_STATE) {
		pinMode(BUTTON_0, INPUT);
	} else {
		pinMode(BUTTON_0, INPUT_PULLUP);
	}
#endif
#ifdef BUTTON_1_ENABLE
	if (BUTTON_1_ACTIVE_STATE) {
		pinMode(BUTTON_1, INPUT);
	} else {
		pinMode(BUTTON_1, INPUT_PULLUP);
	}
#endif
#ifdef BUTTON_2_ENABLE
	if (BUTTON_2_ACTIVE_STATE) {
		pinMode(BUTTON_2, INPUT);
	} else {
		pinMode(BUTTON_2, INPUT_PULLUP);
	}
#endif
#ifdef BUTTON_3_ENABLE
	if (BUTTON_3_ACTIVE_STATE) {
		pinMode(BUTTON_3, INPUT);
	} else {
		pinMode(BUTTON_3, INPUT_PULLUP);
	}
#endif
#ifdef BUTTON_4_ENABLE
	if (BUTTON_4_ACTIVE_STATE) {
		pinMode(BUTTON_4, INPUT);
	} else {
		pinMode(BUTTON_4, INPUT_PULLUP);
	}
#endif
#ifdef BUTTON_5_ENABLE
	if (BUTTON_5_ACTIVE_STATE) {
		pinMode(BUTTON_5, INPUT);
	} else {
		pinMode(BUTTON_5, INPUT_PULLUP);
	}
#endif

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

// Buttons can be mixed between GPIO and port-expander.
// But at the same time only one of them can be for example BUTTON_0
#if defined(BUTTON_0_ENABLE) || defined(EXPANDER_0_ENABLE)
		gButtons[0].currentState = Port_Read(BUTTON_0) ^ BUTTON_0_ACTIVE_STATE;
#endif
#if defined(BUTTON_1_ENABLE) || defined(EXPANDER_1_ENABLE)
		gButtons[1].currentState = Port_Read(BUTTON_1) ^ BUTTON_1_ACTIVE_STATE;
#endif
#if defined(BUTTON_2_ENABLE) || defined(EXPANDER_2_ENABLE)
		gButtons[2].currentState = Port_Read(BUTTON_2) ^ BUTTON_2_ACTIVE_STATE;
#endif
#if defined(BUTTON_3_ENABLE) || defined(EXPANDER_3_ENABLE)
		gButtons[3].currentState = Port_Read(BUTTON_3) ^ BUTTON_3_ACTIVE_STATE;
#endif
#if defined(BUTTON_4_ENABLE) || defined(EXPANDER_4_ENABLE)
		gButtons[4].currentState = Port_Read(BUTTON_4) ^ BUTTON_4_ACTIVE_STATE;
#endif
#if defined(BUTTON_5_ENABLE) || defined(EXPANDER_5_ENABLE)
		gButtons[5].currentState = Port_Read(BUTTON_5) ^ BUTTON_5_ACTIVE_STATE;
#endif

		// Iterate over all buttons in struct-array
		for (uint8_t i = 0; i < sizeof(gButtons) / sizeof(gButtons[0]); i++) {
			if (gButtons[i].currentState != gButtons[i].lastState && currentTimestamp - gButtons[i].lastPressedTimestamp > buttonDebounceInterval) {
				if (!gButtons[i].currentState) {
					gButtons[i].isPressed = true;
					gButtons[i].lastPressedTimestamp = currentTimestamp;
					if (!gButtons[i].firstPressedTimestamp) {
						gButtons[i].firstPressedTimestamp = currentTimestamp;
					}
				} else {
					gButtons[i].isReleased = true;
					gButtons[i].lastReleasedTimestamp = currentTimestamp;
					gButtons[i].firstPressedTimestamp = 0;
				}
			}
			gButtons[i].lastState = gButtons[i].currentState;
		}
	}
	gButtonInitComplete = true;
	Button_DoButtonActions();
}

// Do corresponding actions for all buttons
void Button_DoButtonActions(void) {
	if (gButtons[0].isPressed && gButtons[1].isPressed) {
		gButtons[0].isPressed = false;
		gButtons[1].isPressed = false;
		Cmd_Action(BUTTON_MULTI_01);
	} else if (gButtons[0].isPressed && gButtons[2].isPressed) {
		gButtons[0].isPressed = false;
		gButtons[2].isPressed = false;
		Cmd_Action(BUTTON_MULTI_02);
	} else if (gButtons[0].isPressed && gButtons[3].isPressed) {
		gButtons[0].isPressed = false;
		gButtons[3].isPressed = false;
		Cmd_Action(BUTTON_MULTI_03);
	} else if (gButtons[0].isPressed && gButtons[4].isPressed) {
		gButtons[0].isPressed = false;
		gButtons[4].isPressed = false;
		Cmd_Action(BUTTON_MULTI_04);
	} else if (gButtons[0].isPressed && gButtons[5].isPressed) {
		gButtons[0].isPressed = false;
		gButtons[5].isPressed = false;
		Cmd_Action(BUTTON_MULTI_05);
	} else if (gButtons[1].isPressed && gButtons[2].isPressed) {
		gButtons[1].isPressed = false;
		gButtons[2].isPressed = false;
		Cmd_Action(BUTTON_MULTI_12);
	} else if (gButtons[1].isPressed && gButtons[3].isPressed) {
		gButtons[1].isPressed = false;
		gButtons[3].isPressed = false;
		Cmd_Action(BUTTON_MULTI_13);
	} else if (gButtons[1].isPressed && gButtons[4].isPressed) {
		gButtons[1].isPressed = false;
		gButtons[4].isPressed = false;
		Cmd_Action(BUTTON_MULTI_14);
	} else if (gButtons[1].isPressed && gButtons[5].isPressed) {
		gButtons[1].isPressed = false;
		gButtons[5].isPressed = false;
		Cmd_Action(BUTTON_MULTI_15);
	} else if (gButtons[2].isPressed && gButtons[3].isPressed) {
		gButtons[2].isPressed = false;
		gButtons[3].isPressed = false;
		Cmd_Action(BUTTON_MULTI_23);
	} else if (gButtons[2].isPressed && gButtons[4].isPressed) {
		gButtons[2].isPressed = false;
		gButtons[4].isPressed = false;
		Cmd_Action(BUTTON_MULTI_24);
	} else if (gButtons[2].isPressed && gButtons[5].isPressed) {
		gButtons[2].isPressed = false;
		gButtons[5].isPressed = false;
		Cmd_Action(BUTTON_MULTI_25);
	} else if (gButtons[3].isPressed && gButtons[4].isPressed) {
		gButtons[3].isPressed = false;
		gButtons[4].isPressed = false;
		Cmd_Action(BUTTON_MULTI_34);
	} else if (gButtons[3].isPressed && gButtons[5].isPressed) {
		gButtons[3].isPressed = false;
		gButtons[5].isPressed = false;
		Cmd_Action(BUTTON_MULTI_35);
	} else if (gButtons[4].isPressed && gButtons[5].isPressed) {
		gButtons[4].isPressed = false;
		gButtons[5].isPressed = false;
		Cmd_Action(BUTTON_MULTI_45);
	} else {
		unsigned long currentTimestamp = millis();
		for (uint8_t i = 0; i <= 5; i++) {
			if (gButtons[i].isPressed) {
				uint8_t Cmd_Short = 0;
				uint8_t Cmd_Long = 0;

				switch (i) { // Long-press-actions
					case 0:
						Cmd_Short = BUTTON_0_SHORT;
						Cmd_Long = BUTTON_0_LONG;
						break;

					case 1:
						Cmd_Short = BUTTON_1_SHORT;
						Cmd_Long = BUTTON_1_LONG;
						break;

					case 2:
						Cmd_Short = BUTTON_2_SHORT;
						Cmd_Long = BUTTON_2_LONG;
						break;

					case 3:
						Cmd_Short = BUTTON_3_SHORT;
						Cmd_Long = BUTTON_3_LONG;
						break;

					case 4:
						Cmd_Short = BUTTON_4_SHORT;
						Cmd_Long = BUTTON_4_LONG;
						break;

					case 5:
						Cmd_Short = BUTTON_5_SHORT;
						Cmd_Long = BUTTON_5_LONG;
						break;
				}

				if (gButtons[i].lastReleasedTimestamp > gButtons[i].lastPressedTimestamp) { // short action
					if (gButtons[i].lastReleasedTimestamp - gButtons[i].lastPressedTimestamp < intervalToLongPress) {
						Cmd_Action(Cmd_Short);
					} else {
						// sleep-mode should only be triggered on release, otherwise it will wake it up directly again
						if (Cmd_Long == CMD_SLEEPMODE) {
							Cmd_Action(Cmd_Long);
						}
					}

					gButtons[i].isPressed = false;

				} else if (Cmd_Long == CMD_VOLUMEUP || Cmd_Long == CMD_VOLUMEDOWN) { // volume-buttons
					// only start action if intervalToLongPress has been reached
					if (currentTimestamp - gButtons[i].lastPressedTimestamp > intervalToLongPress) {

						// calculate remainder
						uint16_t remainder = (currentTimestamp - gButtons[i].lastPressedTimestamp) % intervalToLongPress;

						// trigger action if remainder rolled over
						if (remainder < gLongPressTime) {
							Cmd_Action(Cmd_Long);
						}

						gLongPressTime = remainder;
					}

				} else if (Cmd_Long != CMD_SLEEPMODE) { // long action, if not sleep-mode
					// start action if intervalToLongPress has been reached
					if ((currentTimestamp - gButtons[i].lastPressedTimestamp) > intervalToLongPress) {
						gButtons[i].isPressed = false;
						Cmd_Action(Cmd_Long);
					}
				}
			}
		}
	}
}

void IRAM_ATTR onTimer() {
	xSemaphoreGiveFromISR(Button_TimerSemaphore, NULL);
}
