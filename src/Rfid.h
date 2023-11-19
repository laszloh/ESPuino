#pragma once

#include "RfidEvent.h"

namespace rfid {

void init();
void cyclic();
void exit();
void taskPause();
void taskResume();
void wakeupCheck();

void resetOldRfid();

CardIdType &getCurrentRfidTagId();
void forceEvent(const Message::Event event, const CardIdType &cardId = {});

} // namespace rfid
