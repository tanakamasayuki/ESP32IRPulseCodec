#include "ESP32IRPulseCodec.h"
#include <esp_log.h>

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
        // TODO: Implement RMT transmit path and honor gapUs_/invertOutput_/carrierHz_.
        ESP_LOGW(kTag, "TX send ITPS not implemented yet");
        return false;
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
        // TODO: encode ProtocolMessage into ITPSBuffer and dispatch to TX HAL.
        ESP_LOGW(kTag, "TX send ProtocolMessage not implemented yet");
        return false;
    }

} // namespace esp32ir
