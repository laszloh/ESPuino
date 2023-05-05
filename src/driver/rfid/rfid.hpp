#pragma once

#include "crtp.hpp"
#include "settings.h"
#include <WString.h>
#include <mutex>

namespace rfid::driver
{
struct Message {
    static constexpr size_t cardIdSize = 4;

    enum class Event : uint8_t {
        NoCard = 0,
        CardRemoved,
        CardApplied,
        CardPresent
    };

    Event event;
    uint8_t cardId[cardIdSize];

    const String toString() const {
        char buf[3 * cardIdSize] = {0};

        for(int i = 0; i < cardIdSize; i++) {
            const size_t idx = i * 3;
            buf[idx] = cardId[i] / 100;
            buf[idx + 1] = (cardId[i] % 100) / 10;
            buf[idx + 2] = (cardId[i] % 100) % 10;
        }

        return buf;
    }

    const String toHexString() const {
        char buf[3 * cardIdSize] = {0};

        constexpr char hexDigits[] = "0123456789ABCDEF";
        for(size_t i = 0; i < cardIdSize; i++) {
            const size_t idx = i * 3;
            buf[idx] = hexDigits[(cardId[i]>>4) & 0x0F];
            buf[idx + 1] = hexDigits[cardId[i] & 0x0F];
            buf[idx + 2] = (i < cardIdSize - 1) ? '-' : '\0';
        }

        return buf;
    }

};

template<typename T>
class RfidDriverBase : crtp<T, RfidDriverBase> {
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


    protected:
        SemaphoreHandle_t cardChangeEvent{xSemaphoreCreateBinary()};
        std::mutex accessGuard;
        Message message;

        void signalEvent(Message::Event event, uint8_t cardId[] = nullptr) {
            // get the access guard
            std::lock_guard guard(accessGuard);
            message.event = event;
            if(cardId) {
                memcpy(message.cardId, cardId, Message::cardIdSize);
            } else {
                memset(message.cardId, 0, sizeof(message.cardId));
            }

            // signal the event
            xSemaphoreGive(cardChangeEvent);
        }

        void signalEvent(Message msg) {
            std::lock_guard guard(accessGuard);
            message = msg;

            // signal the event
            xSemaphoreGive(cardChangeEvent);
        }

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

