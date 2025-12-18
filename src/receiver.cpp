#include "ESP32IRPulseCodec.h"
#include <esp_log.h>
#ifdef ESP_PLATFORM
#include <driver/rmt.h>
#endif

namespace esp32ir
{

    namespace
    {
        constexpr const char *kTag = "ESP32IRPulseCodec";

        bool isACProtocol(esp32ir::Protocol p)
        {
            switch (p)
            {
            case esp32ir::Protocol::DaikinAC:
            case esp32ir::Protocol::PanasonicAC:
            case esp32ir::Protocol::MitsubishiAC:
            case esp32ir::Protocol::ToshibaAC:
            case esp32ir::Protocol::FujitsuAC:
                return true;
            default:
                return false;
            }
        }

        std::vector<esp32ir::Protocol> allKnownProtocols()
        {
            return {
                esp32ir::Protocol::NEC,
                esp32ir::Protocol::SONY,
                esp32ir::Protocol::AEHA,
                esp32ir::Protocol::Panasonic,
                esp32ir::Protocol::JVC,
                esp32ir::Protocol::Samsung,
                esp32ir::Protocol::LG,
                esp32ir::Protocol::Denon,
                esp32ir::Protocol::RC5,
                esp32ir::Protocol::RC6,
                esp32ir::Protocol::Apple,
                esp32ir::Protocol::Pioneer,
                esp32ir::Protocol::Toshiba,
                esp32ir::Protocol::Mitsubishi,
                esp32ir::Protocol::Hitachi,
                esp32ir::Protocol::DaikinAC,
                esp32ir::Protocol::PanasonicAC,
                esp32ir::Protocol::MitsubishiAC,
                esp32ir::Protocol::ToshibaAC,
                esp32ir::Protocol::FujitsuAC};
        }

        std::vector<esp32ir::Protocol> knownWithoutAC()
        {
            std::vector<esp32ir::Protocol> v;
            for (auto p : allKnownProtocols())
            {
                if (!isACProtocol(p))
                {
                    v.push_back(p);
                }
            }
            return v;
        }

        void dedupAppend(std::vector<esp32ir::Protocol> &list, esp32ir::Protocol p)
        {
            for (auto existing : list)
            {
                if (existing == p)
                {
                    return;
                }
            }
            list.push_back(p);
        }
    } // namespace

    Receiver::Receiver() = default;
    Receiver::Receiver(int pin, bool invert, uint16_t T_us_rx)
        : rxPin_(pin), invertInput_(invert), quantizeT_(T_us_rx) {}

    bool Receiver::setPin(int pin)
    {
        if (begun_)
            return false;
        rxPin_ = pin;
        return true;
    }
    bool Receiver::setInvertInput(bool invert)
    {
        if (begun_)
            return false;
        invertInput_ = invert;
        return true;
    }
    bool Receiver::setQuantizeT(uint16_t T_us_rx)
    {
        if (begun_)
            return false;
        quantizeT_ = T_us_rx;
        return true;
    }

    bool Receiver::begin()
    {
        if (begun_)
        {
            ESP_LOGW(kTag, "RX begin called while already begun");
            return false;
        }
        if (rxPin_ < 0)
        {
            ESP_LOGE(kTag, "RX begin failed: pin not set");
            return false;
        }
#ifdef ESP_PLATFORM
        rmt_config_t config = {};
        config.rmt_mode = RMT_MODE_RX;
        config.channel = static_cast<rmt_channel_t>(rmtChannel_);
        config.gpio_num = rxPin_;
        config.mem_block_num = 2;
        config.clk_div = 80; // 1us tick
        config.rx_config.filter_en = true;
        config.rx_config.filter_ticks_thresh = quantizeT_;
        config.rx_config.idle_threshold = 60000; // 60ms split
        if (invertInput_)
        {
            config.rx_config.filter_en = false; // let raw pass to handle inversion in software if needed
        }
        if (rmt_config(&config) != ESP_OK)
        {
            ESP_LOGE(kTag, "RX begin failed: rmt_config");
            return false;
        }
        if (rmt_driver_install(config.channel, 1000, 0) != ESP_OK)
        {
            ESP_LOGE(kTag, "RX begin failed: rmt_driver_install");
            return false;
        }
        if (rmt_get_ringbuf_handle(static_cast<rmt_channel_t>(rmtChannel_), &rmtRingbuf_) != ESP_OK || !rmtRingbuf_)
        {
            ESP_LOGE(kTag, "RX begin failed: rmt_get_ringbuf_handle");
            rmt_driver_uninstall(static_cast<rmt_channel_t>(rmtChannel_));
            return false;
        }
        rmt_rx_start(static_cast<rmt_channel_t>(rmtChannel_), true);
#endif

        if (!useRawOnly_)
        {
            if (protocols_.empty())
            {
                protocols_ = useKnownNoAC_ ? knownWithoutAC() : allKnownProtocols();
            }
            else if (useKnownNoAC_)
            {
                std::vector<esp32ir::Protocol> filtered;
                for (auto p : protocols_)
                {
                    if (!isACProtocol(p))
                    {
                        dedupAppend(filtered, p);
                    }
                }
                protocols_.swap(filtered);
            }
        }
        begun_ = true;
        ESP_LOGI(kTag, "RX begin: pin=%d invert=%s T_us=%u mode=%s",
                 rxPin_, invertInput_ ? "true" : "false", static_cast<unsigned>(quantizeT_),
                 useRawOnly_ ? "RAW_ONLY" : (useRawPlusKnown_ ? "RAW_PLUS_KNOWN" : (useKnownNoAC_ ? "KNOWN_NO_AC" : "KNOWN_ONLY")));
        return true;
    }
    void Receiver::end()
    {
        if (!begun_)
        {
            return;
        }
#ifdef ESP_PLATFORM
        rmt_rx_stop(static_cast<rmt_channel_t>(rmtChannel_));
        rmt_driver_uninstall(static_cast<rmt_channel_t>(rmtChannel_));
        rmtRingbuf_ = nullptr;
#endif
        begun_ = false;
        ESP_LOGI(kTag, "RX end");
    }

    bool Receiver::addProtocol(esp32ir::Protocol protocol)
    {
        if (begun_)
            return false;
        dedupAppend(protocols_, protocol);
        return true;
    }
    bool Receiver::clearProtocols()
    {
        if (begun_)
            return false;
        protocols_.clear();
        return true;
    }
    bool Receiver::useRawOnly()
    {
        if (begun_)
            return false;
        useRawOnly_ = true;
        useRawPlusKnown_ = false;
        return true;
    }
    bool Receiver::useRawPlusKnown()
    {
        if (begun_)
            return false;
        useRawOnly_ = false;
        useRawPlusKnown_ = true;
        return true;
    }
    bool Receiver::useKnownWithoutAC()
    {
        if (begun_)
            return false;
        useKnownNoAC_ = true;
        return true;
    }

    namespace
    {
#ifdef ESP_PLATFORM
        void pushSeq(std::vector<int8_t> &seq, bool mark, uint32_t durationUs, uint16_t T_us)
        {
            if (T_us == 0 || durationUs == 0)
            {
                return;
            }
            uint32_t counts = (durationUs + (T_us / 2)) / T_us;
            if (counts == 0)
            {
                counts = 1;
            }
            while (counts > 127)
            {
                seq.push_back(mark ? 127 : -127);
                counts -= 127;
            }
            seq.push_back(static_cast<int8_t>(mark ? counts : -static_cast<int>(counts)));
        }
#endif
    } // namespace

    bool Receiver::poll(esp32ir::RxResult &out)
    {
        static bool warned = false;
        if (!begun_)
        {
            ESP_LOGW(kTag, "RX poll called before begin");
        }
#ifdef ESP_PLATFORM
        if (!rmtRingbuf_)
        {
            return false;
        }
        size_t itemSize = 0;
        rmt_item32_t *items = (rmt_item32_t *)xRingbufferReceive(rmtRingbuf_, &itemSize, 0);
        if (!items || itemSize == 0)
        {
            return false;
        }
        uint32_t count = itemSize / sizeof(rmt_item32_t);
        std::vector<int8_t> seq;
        seq.reserve(count * 2);
        for (uint32_t i = 0; i < count; ++i)
        {
            const auto &it = items[i];
            if (it.duration0)
            {
                bool mark = invertInput_ ? (it.level0 == 0) : (it.level0 != 0);
                pushSeq(seq, mark, it.duration0, quantizeT_);
            }
            if (it.duration1)
            {
                bool mark = invertInput_ ? (it.level1 == 0) : (it.level1 != 0);
                pushSeq(seq, mark, it.duration1, quantizeT_);
            }
        }
        vRingbufferReturnItem(rmtRingbuf_, (void *)items);
        while (!seq.empty() && seq.front() < 0)
        {
            seq.erase(seq.begin());
        }
        if (seq.empty() || seq.front() < 0)
        {
            return false;
        }
        esp32ir::ITPSFrame frame{quantizeT_, static_cast<uint16_t>(seq.size()), seq.data(), 0};
        esp32ir::ITPSBuffer buf;
        buf.addFrame(frame);

        auto protocolsToTry = protocols_;
        if (protocolsToTry.empty())
        {
            protocolsToTry = useKnownNoAC_ ? knownWithoutAC() : allKnownProtocols();
        }

        auto fillDecoded = [&](esp32ir::Protocol proto, const void *payload, size_t len) -> bool
        {
            out.payloadStorage.assign(reinterpret_cast<const uint8_t *>(payload),
                                      reinterpret_cast<const uint8_t *>(payload) + len);
            out.message = {proto, out.payloadStorage.data(), static_cast<uint16_t>(len), 0};
            out.protocol = proto;
            out.status = esp32ir::RxStatus::DECODED;
            if (useRawPlusKnown_)
            {
                out.raw = buf;
            }
            else
            {
                out.raw.clear();
            }
            return true;
        };

        esp32ir::RxResult temp;
        temp.status = esp32ir::RxStatus::RAW_ONLY;
        temp.protocol = esp32ir::Protocol::RAW;
        temp.message = {esp32ir::Protocol::RAW, nullptr, 0, 0};
        temp.raw = buf;

        for (auto proto : protocolsToTry)
        {
            switch (proto)
            {
            case esp32ir::Protocol::NEC:
            {
                esp32ir::payload::NEC p{};
                if (esp32ir::decodeNEC(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::SONY:
            {
                esp32ir::payload::SONY p{};
                if (esp32ir::decodeSONY(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::AEHA:
            {
                esp32ir::payload::AEHA p{};
                if (esp32ir::decodeAEHA(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::Panasonic:
            {
                esp32ir::payload::Panasonic p{};
                if (esp32ir::decodePanasonic(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::JVC:
            {
                esp32ir::payload::JVC p{};
                if (esp32ir::decodeJVC(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::Samsung:
            {
                esp32ir::payload::Samsung p{};
                if (esp32ir::decodeSamsung(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::LG:
            {
                esp32ir::payload::LG p{};
                if (esp32ir::decodeLG(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::Denon:
            {
                esp32ir::payload::Denon p{};
                if (esp32ir::decodeDenon(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::RC5:
            {
                esp32ir::payload::RC5 p{};
                if (esp32ir::decodeRC5(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::RC6:
            {
                esp32ir::payload::RC6 p{};
                if (esp32ir::decodeRC6(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::Apple:
            {
                esp32ir::payload::Apple p{};
                if (esp32ir::decodeApple(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::Pioneer:
            {
                esp32ir::payload::Pioneer p{};
                if (esp32ir::decodePioneer(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::Toshiba:
            {
                esp32ir::payload::Toshiba p{};
                if (esp32ir::decodeToshiba(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::Mitsubishi:
            {
                esp32ir::payload::Mitsubishi p{};
                if (esp32ir::decodeMitsubishi(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            case esp32ir::Protocol::Hitachi:
            {
                esp32ir::payload::Hitachi p{};
                if (esp32ir::decodeHitachi(temp, p))
                {
                    return fillDecoded(proto, &p, sizeof(p));
                }
                break;
            }
            default:
                break;
            }
        }
        // No decode hit
        if (useRawPlusKnown_ || useRawOnly_)
        {
            out.status = esp32ir::RxStatus::RAW_ONLY;
            out.protocol = esp32ir::Protocol::RAW;
            out.message = {esp32ir::Protocol::RAW, nullptr, 0, 0};
            out.raw = buf;
            out.payloadStorage.clear();
            return true;
        }
        return false;
#else
        if (!warned)
        {
            ESP_LOGW(kTag, "RX poll failed: HAL decode pipeline not available in this build");
            warned = true;
        }
        return false;
#endif
    }

} // namespace esp32ir
