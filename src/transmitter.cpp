#include "ESP32IRPulseCodec.h"
#include <esp_log.h>
#include <cstring>

namespace esp32ir
{

    namespace
    {
        constexpr const char *kTag = "ESP32IRPulseCodec";

        bool itpsFrameValid(const esp32ir::ITPSFrame &f)
        {
            if (f.T_us == 0 || f.len == 0 || f.seq == nullptr)
            {
                return false;
            }
            for (uint16_t i = 0; i < f.len; ++i)
            {
                int v = f.seq[i];
                if (v == 0 || v > 127 || v < -127)
                {
                    return false;
                }
            }
            return true;
        }

        bool itpsValid(const esp32ir::ITPSBuffer &b)
        {
            if (b.frameCount() == 0)
            {
                return false;
            }
            for (uint16_t i = 0; i < b.frameCount(); ++i)
            {
                if (!itpsFrameValid(b.frame(i)))
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace

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
        if (begun_)
        {
            ESP_LOGW(kTag, "TX begin called while already begun");
            return false;
        }
        if (txPin_ < 0)
        {
            ESP_LOGE(kTag, "TX begin failed: pin not set");
            return false;
        }
        begun_ = true;
        ESP_LOGI(kTag, "TX begin: pin=%d invert=%s carrier=%luHz duty=%u%% gapUs=%lu",
                 txPin_, invertOutput_ ? "true" : "false",
                 static_cast<unsigned long>(carrierHz_),
                 static_cast<unsigned>(dutyPercent_),
                 static_cast<unsigned long>(gapUs_));
        return true;
    }
    void Transmitter::end()
    {
        if (!begun_)
        {
            return;
        }
        begun_ = false;
        ESP_LOGI(kTag, "TX end");
    }

    bool Transmitter::send(const esp32ir::ITPSBuffer &itps)
    {
        if (!begun_)
        {
            ESP_LOGE(kTag, "TX send called before begin");
            return false;
        }
        if (!itpsValid(itps))
        {
            ESP_LOGE(kTag, "TX send failed: invalid ITPSBuffer");
            return false;
        }
        ESP_LOGW(kTag, "TX send ITPS stub: hardware TX path not implemented yet");
        return true;
    }
    bool Transmitter::send(const esp32ir::ProtocolMessage &message)
    {
        if (!begun_)
        {
            ESP_LOGE(kTag, "TX send (protocol) called before begin");
            return false;
        }
        if (message.data == nullptr || message.length == 0)
        {
            ESP_LOGE(kTag, "TX send failed: protocol payload missing");
            return false;
        }
        auto checkSizeStub = [&](size_t expected, const char *name) -> bool
        {
            if (message.length != expected)
            {
                ESP_LOGE(kTag, "TX send %s failed: size mismatch (got %u expected %u)",
                         name, static_cast<unsigned>(message.length), static_cast<unsigned>(expected));
                return false;
            }
            ESP_LOGW(kTag, "TX send %s stub: encode/HAL not implemented", name);
            return true;
        };
        switch (message.protocol)
        {
        case esp32ir::Protocol::NEC:
        {
            if (message.length != sizeof(esp32ir::payload::NEC))
            {
                ESP_LOGE(kTag, "TX send NEC failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::NEC)));
                return false;
            }
            esp32ir::payload::NEC p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendNEC(p);
        }
        case esp32ir::Protocol::SONY:
        {
            if (message.length != sizeof(esp32ir::payload::SONY))
            {
                ESP_LOGE(kTag, "TX send SONY failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::SONY)));
                return false;
            }
            esp32ir::payload::SONY p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendSONY(p);
        }
        case esp32ir::Protocol::AEHA:
            return checkSizeStub(sizeof(esp32ir::payload::AEHA), "AEHA");
        case esp32ir::Protocol::Panasonic:
            return checkSizeStub(sizeof(esp32ir::payload::Panasonic), "Panasonic");
        case esp32ir::Protocol::JVC:
            return checkSizeStub(sizeof(esp32ir::payload::JVC), "JVC");
        case esp32ir::Protocol::Samsung:
            return checkSizeStub(sizeof(esp32ir::payload::Samsung), "Samsung");
        case esp32ir::Protocol::LG:
            return checkSizeStub(sizeof(esp32ir::payload::LG), "LG");
        case esp32ir::Protocol::Denon:
            return checkSizeStub(sizeof(esp32ir::payload::Denon), "Denon");
        case esp32ir::Protocol::RC5:
            return checkSizeStub(sizeof(esp32ir::payload::RC5), "RC5");
        case esp32ir::Protocol::RC6:
            return checkSizeStub(sizeof(esp32ir::payload::RC6), "RC6");
        case esp32ir::Protocol::Apple:
            return checkSizeStub(sizeof(esp32ir::payload::Apple), "Apple");
        case esp32ir::Protocol::Pioneer:
            return checkSizeStub(sizeof(esp32ir::payload::Pioneer), "Pioneer");
        case esp32ir::Protocol::Toshiba:
            return checkSizeStub(sizeof(esp32ir::payload::Toshiba), "Toshiba");
        case esp32ir::Protocol::Mitsubishi:
            return checkSizeStub(sizeof(esp32ir::payload::Mitsubishi), "Mitsubishi");
        case esp32ir::Protocol::Hitachi:
            return checkSizeStub(sizeof(esp32ir::payload::Hitachi), "Hitachi");
        case esp32ir::Protocol::DaikinAC:
            return checkSizeStub(sizeof(esp32ir::payload::DaikinAC), "DaikinAC");
        case esp32ir::Protocol::PanasonicAC:
            return checkSizeStub(sizeof(esp32ir::payload::PanasonicAC), "PanasonicAC");
        case esp32ir::Protocol::MitsubishiAC:
            return checkSizeStub(sizeof(esp32ir::payload::MitsubishiAC), "MitsubishiAC");
        case esp32ir::Protocol::ToshibaAC:
            return checkSizeStub(sizeof(esp32ir::payload::ToshibaAC), "ToshibaAC");
        case esp32ir::Protocol::FujitsuAC:
            return checkSizeStub(sizeof(esp32ir::payload::FujitsuAC), "FujitsuAC");
        default:
            ESP_LOGW(kTag, "TX send ProtocolMessage stub: encode/HAL not implemented (protocol=%u, len=%u)",
                     static_cast<unsigned>(message.protocol),
                     static_cast<unsigned>(message.length));
            return true;
        }
    }

} // namespace esp32ir
