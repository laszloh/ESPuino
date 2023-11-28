#pragma once

#include <Arduino.h>
#include "settings.h"

#include "values.h"

struct RfidEntry {
	String file;
	uint32_t lastPlayPos;
	uint16_t trackLastPlayed;
	CardActions playMode;
};
