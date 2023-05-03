#pragma once

#include "crtp.hpp"
#include "settings.h"
#include <WString.h>

namespace rfid::driver
{

template<typename T>
class RfidDriverBase : crtp<T, RfidDriver> {
    public:
        void init() {
            this->underlying().init();
        }

        SemaphoreHandle_t &getCardChangeSemaphore() {
            return cardChangeEvent;
        }

        bool waitForCardEvent(uint32_t ms) {
            return xSemaphoreTake(cardChangeEvent, ms / portTICK_PERIOD_MS);
        }

        bool isCardPresent() {
            this->underlying().isCardPresent();
        }

        const String getCardId() {
            this->underlying().getCardId();
        }

    protected:
        SemaphoreHandle_t cardChangeEvent{xSemaphoreCreateBinary()};

    private:
        RfidDriver() {}
        friend T;
};

// select a driver driver here, the driver is then instanciated with "RfidDriver rfid;" in Rfid.cpp
#if defined(RFID_READER_TYPE_PN5180)
    #include "pn5180.hpp"
#elif defined(RFID_READER_TYPE_MFRC522)
    #include "mfrc522.hpp"
#elif defined(RFID_READER_TYPE_PN532)
    #include "pn532.hpp"
#else
    #warning No RFID card presendriver selected
    #undef RFID_READER_ENABLED
#endif

} // namespace rfid::driver
