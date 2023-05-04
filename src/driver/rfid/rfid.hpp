#pragma once

#include "crtp.hpp"
#include "settings.h"
#include <WString.h>
#include <mutex>

namespace rfid::driver
{

template<typename T>
class RfidDriverBase : crtp<T, RfidDriverBase> {
    public:
        static constexpr size_t cardIdSize = 4;

        struct Message {
            enum class Event : uint8_t {
                NoCard = 0,
                CardRemoved,
                CardApplied,
                CardPresent
            };

            Event event;
            uint8_t cardId[cardIdSize];
        };

    public:
        void init() {
            this->underlying().init();
        }

        const Message &getLastEvent() {
            std::lock_guard guard(accessGuard);
            return message;
        }

        void suspend(bool enable) {
            this->underlying().suspend(enable);
        }

        void wakeupCheck() {
            this->underlying().wakeupCheck();
        }

        bool waitForCardEvent(uint32_t ms) {
            return xSemaphoreTake(cardChangeEvent, ms / portTICK_PERIOD_MS);
        }

        static const String binToHex(uint8_t *buffer, size_t len) {
            char buf[3 * len] = {0};

            constexpr char hexDigits[] = "0123456789ABCDEF";
            for(size_t i = 0; i < len; i++) {
                const size_t idx = i * 3;
                buf[idx] = hexDigits[(buffer[i]>>4) & 0x0F];
                buf[idx + 1] = hexDigits[buffer[i] & 0x0F];
                buf[idx + 2] = (i < len -1) ? '-' : '\0';
            }

            return buf;
        }

        static const String binToDec(uint8_t *buffer, size_t len) {
            char buf[3 * len] = {0};

            for(int i = 0; i < len; i++) {
                const size_t idx = i * 3;
                buf[idx] = buffer[i] / 100;
                buf[idx + 1] = (buffer[i] % 100) / 10;
                buf[idx + 2] = (buffer[i] % 100) % 10;
            }

            return buf;
        }

    protected:
        SemaphoreHandle_t cardChangeEvent{xSemaphoreCreateBinary()};
        std::mutex accessGuard;
        Message message;

    private:
        RfidDriverBase() {}
        friend T;
};

} // namespace rfid::driver

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

