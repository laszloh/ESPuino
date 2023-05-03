#pragma once

#include "rfid.hpp"

namespace rfid::driver {
namespace implementation {

class RfidPN1580 : public RfidDriverBase<RfidPN1580> {
    using RfidDriverBase::cardChangeEvent;

    public:

        void init() { }

        bool isCardPresent() {
            return true;
        }

        const String getCardId() {
            return "123456789012";
        }
};

} // namespace implementation

using RfidDriver = implementation::RfidPN1580;

} // namespace rfid::driver
