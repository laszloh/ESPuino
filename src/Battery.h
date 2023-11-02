#pragma once

extern uint8_t batteryCheckInterval;

extern float voltageIndicatorCritical;
extern float warningLowVoltage;
extern float warningCriticalVoltage;
extern float voltageIndicatorLow;
extern float voltageIndicatorHigh;

void Battery_Init(void);
void Battery_Cyclic(void);

float Battery_EstimateLevel(void);
float Battery_GetVoltage(void);
bool Battery_IsLow(void);
bool Battery_IsCritical(void);

void Battery_PublishMQTT(void);
void Battery_LogStatus(void);

// Implementation specific tasks
void Battery_CyclicInner(void);
void Battery_InitInner(void);
