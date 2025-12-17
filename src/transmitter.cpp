#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    Transmitter::Transmitter() = default;
    Transmitter::Transmitter(int pin, bool invert, uint32_t hz)
        : txPin_(pin), invertOutput_(invert), carrierHz_(hz) {}

    bool Transmitter::setPin(int pin)
    {
        if (begun_)
            return false;
        txPin_ = pin;
        return true;
    }
    bool Transmitter::setInvertOutput(bool invert)
    {
        if (begun_)
            return false;
        invertOutput_ = invert;
        return true;
    }
    bool Transmitter::setCarrierHz(uint32_t hz)
    {
        if (begun_)
            return false;
        carrierHz_ = hz;
        return true;
    }
    bool Transmitter::setDutyPercent(uint8_t dutyPercent)
    {
        if (begun_)
            return false;
        dutyPercent_ = dutyPercent;
        return true;
    }
    bool Transmitter::setGapUs(uint32_t gapUs)
    {
        if (begun_)
            return false;
        gapUs_ = gapUs;
        return true;
    }

    bool Transmitter::begin()
    {
        if (txPin_ < 0)
            return false;
        begun_ = true;
        return true;
    }
    void Transmitter::end() { begun_ = false; }

    bool Transmitter::send(const esp32ir::ITPSBuffer &) { return false; }
    bool Transmitter::send(const esp32ir::ProtocolMessage &) { return false; }

} // namespace esp32ir
