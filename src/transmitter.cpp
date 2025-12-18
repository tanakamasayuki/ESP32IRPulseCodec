#include "ESP32IRPulseCodec.h"
#include <esp_log.h>
#include <cstring>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>

namespace esp32ir
{

    namespace
    {
        constexpr const char *kTag = "ESP32IRPulseCodec";

        constexpr uint32_t kRmtResolutionHz = 1000000; // 1us tick
        constexpr uint32_t kRmtDurationMax = 32767;

        void pushSymbol(std::vector<rmt_symbol_word_t> &items, bool level, uint32_t durationUs)
        {
            uint32_t remaining = durationUs;
            while (remaining > 0)
            {
                uint32_t chunk = remaining > kRmtDurationMax ? kRmtDurationMax : remaining;
                if (items.empty() || items.back().duration1 != 0)
                {
                    rmt_symbol_word_t item = {};
                    item.level0 = level ? 1 : 0;
                    item.duration0 = chunk;
                    items.push_back(item);
                }
                else
                {
                    rmt_symbol_word_t &last = items.back();
                    last.level1 = level ? 1 : 0;
                    last.duration1 = chunk;
                }
                remaining -= chunk;
            }
        }

        void appendITPSFrame(std::vector<rmt_symbol_word_t> &items, const esp32ir::ITPSFrame &f)
        {
            if (!f.seq || f.len == 0 || f.T_us == 0)
            {
                return;
            }
            for (uint16_t i = 0; i < f.len; ++i)
            {
                int v = f.seq[i];
                uint32_t durUs = static_cast<uint32_t>((v < 0) ? -v : v) * static_cast<uint32_t>(f.T_us);
                bool level = v > 0;
                pushSymbol(items, level, durUs);
            }
        }

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
        rmt_tx_channel_config_t config = {
            .gpio_num = static_cast<gpio_num_t>(txPin_),
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = kRmtResolutionHz,
            .mem_block_symbols = 64,
            .trans_queue_depth = 4,
            .flags = {
                .invert_out = invertOutput_ ? 1 : 0,
            },
        };

        if (rmt_new_tx_channel(&config, &txChannel_) != ESP_OK)
        {
            ESP_LOGE(kTag, "TX begin failed: rmt_new_tx_channel");
            return false;
        }
        if (rmt_enable(txChannel_) != ESP_OK)
        {
            ESP_LOGE(kTag, "TX begin failed: rmt_enable");
            rmt_del_channel(txChannel_);
            txChannel_ = nullptr;
            return false;
        }
        if (carrierHz_ > 0)
        {
            uint32_t duty = dutyPercent_ == 0 ? 50 : dutyPercent_;
            rmt_carrier_config_t carrier_cfg = {
                .frequency_hz = carrierHz_,
                .duty_cycle = duty,
                .flags = {
                    .polarity_active_low = 0,
                    .always_on = 0,
                },
            };
            if (rmt_apply_carrier(txChannel_, &carrier_cfg) != ESP_OK)
            {
                ESP_LOGE(kTag, "TX begin failed: rmt_apply_carrier");
                rmt_del_channel(txChannel_);
                txChannel_ = nullptr;
                return false;
            }
        }
        rmt_copy_encoder_config_t enc_cfg = {};
        if (rmt_new_copy_encoder(&enc_cfg, &txEncoder_) != ESP_OK)
        {
            ESP_LOGE(kTag, "TX begin failed: rmt_new_copy_encoder");
            rmt_del_channel(txChannel_);
            txChannel_ = nullptr;
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
        if (txEncoder_)
        {
            rmt_del_encoder(txEncoder_);
            txEncoder_ = nullptr;
        }
        if (txChannel_)
        {
            rmt_disable(txChannel_);
            rmt_del_channel(txChannel_);
            txChannel_ = nullptr;
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
        std::vector<rmt_symbol_word_t> items;
        for (uint16_t i = 0; i < itps.frameCount(); ++i)
        {
            appendITPSFrame(items, itps.frame(i));
        }
        // enforce trailing gap as Space
        if (gapUs_ > 0)
        {
            pushSymbol(items, false, gapUs_);
        }
        if (items.empty())
        {
            ESP_LOGE(kTag, "TX send failed: empty RMT items");
            return false;
        }
        rmt_transmit_config_t tx_cfg = {
            .loop_count = 0,
        };
        esp_err_t err = rmt_transmit(txChannel_, txEncoder_, items.data(),
                                     items.size() * sizeof(rmt_symbol_word_t), &tx_cfg);
        if (err != ESP_OK)
        {
            ESP_LOGE(kTag, "TX send failed: rmt_transmit err=%d", err);
            return false;
        }
        err = rmt_tx_wait_all_done(txChannel_, pdMS_TO_TICKS(gapUs_ / 1000 + 50));
        if (err != ESP_OK)
        {
            ESP_LOGW(kTag, "TX wait done returned err=%d", err);
        }
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
            return false;
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
        {
            if (message.length != sizeof(esp32ir::payload::AEHA))
            {
                ESP_LOGE(kTag, "TX send AEHA failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::AEHA)));
                return false;
            }
            esp32ir::payload::AEHA p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendAEHA(p);
        }
        case esp32ir::Protocol::Panasonic:
        {
            if (message.length != sizeof(esp32ir::payload::Panasonic))
            {
                ESP_LOGE(kTag, "TX send Panasonic failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::Panasonic)));
                return false;
            }
            esp32ir::payload::Panasonic p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendPanasonic(p);
        }
        case esp32ir::Protocol::JVC:
        {
            if (message.length != sizeof(esp32ir::payload::JVC))
            {
                ESP_LOGE(kTag, "TX send JVC failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::JVC)));
                return false;
            }
            esp32ir::payload::JVC p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendJVC(p);
        }
        case esp32ir::Protocol::Samsung:
        {
            if (message.length != sizeof(esp32ir::payload::Samsung))
            {
                ESP_LOGE(kTag, "TX send Samsung failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::Samsung)));
                return false;
            }
            esp32ir::payload::Samsung p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendSamsung(p);
        }
        case esp32ir::Protocol::LG:
        {
            if (message.length != sizeof(esp32ir::payload::LG))
            {
                ESP_LOGE(kTag, "TX send LG failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::LG)));
                return false;
            }
            esp32ir::payload::LG p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendLG(p);
        }
        case esp32ir::Protocol::Denon:
        {
            if (message.length != sizeof(esp32ir::payload::Denon))
            {
                ESP_LOGE(kTag, "TX send Denon failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::Denon)));
                return false;
            }
            esp32ir::payload::Denon p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendDenon(p);
        }
        case esp32ir::Protocol::RC5:
        {
            if (message.length != sizeof(esp32ir::payload::RC5))
            {
                ESP_LOGE(kTag, "TX send RC5 failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::RC5)));
                return false;
            }
            esp32ir::payload::RC5 p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendRC5(p);
        }
        case esp32ir::Protocol::RC6:
        {
            if (message.length != sizeof(esp32ir::payload::RC6))
            {
                ESP_LOGE(kTag, "TX send RC6 failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::RC6)));
                return false;
            }
            esp32ir::payload::RC6 p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendRC6(p);
        }
        case esp32ir::Protocol::Apple:
        {
            if (message.length != sizeof(esp32ir::payload::Apple))
            {
                ESP_LOGE(kTag, "TX send Apple failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::Apple)));
                return false;
            }
            esp32ir::payload::Apple p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendApple(p);
        }
        case esp32ir::Protocol::Pioneer:
        {
            if (message.length != sizeof(esp32ir::payload::Pioneer))
            {
                ESP_LOGE(kTag, "TX send Pioneer failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::Pioneer)));
                return false;
            }
            esp32ir::payload::Pioneer p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendPioneer(p);
        }
        case esp32ir::Protocol::Toshiba:
        {
            if (message.length != sizeof(esp32ir::payload::Toshiba))
            {
                ESP_LOGE(kTag, "TX send Toshiba failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::Toshiba)));
                return false;
            }
            esp32ir::payload::Toshiba p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendToshiba(p);
        }
        case esp32ir::Protocol::Mitsubishi:
        {
            if (message.length != sizeof(esp32ir::payload::Mitsubishi))
            {
                ESP_LOGE(kTag, "TX send Mitsubishi failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::Mitsubishi)));
                return false;
            }
            esp32ir::payload::Mitsubishi p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendMitsubishi(p);
        }
        case esp32ir::Protocol::Hitachi:
        {
            if (message.length != sizeof(esp32ir::payload::Hitachi))
            {
                ESP_LOGE(kTag, "TX send Hitachi failed: size mismatch (got %u expected %u)",
                         static_cast<unsigned>(message.length), static_cast<unsigned>(sizeof(esp32ir::payload::Hitachi)));
                return false;
            }
            esp32ir::payload::Hitachi p;
            std::memcpy(&p, message.data, sizeof(p));
            return sendHitachi(p);
        }
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
            return false;
        }
    }

} // namespace esp32ir
