#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/protocol_message_utils.h"
#include <algorithm>
#include <cctype>

namespace esp32ir
{
    namespace
    {
        std::string toLower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        Protocol protocolFromCapabilities(const esp32ir::ac::Capabilities &caps)
        {
            auto hintMatches = [](const std::string &value, const std::string &needle) {
                return value.find(needle) != std::string::npos;
            };

            std::string vendor = toLower(caps.device.vendor);
            std::string brand = caps.device.brand ? toLower(*caps.device.brand) : "";
            std::string hint = caps.device.protocolHint ? toLower(*caps.device.protocolHint) : "";

            auto has = [&](const std::string &needle) {
                return hintMatches(vendor, needle) || hintMatches(brand, needle) || hintMatches(hint, needle);
            };

            if (has("daikin"))
                return Protocol::DaikinAC;
            if (has("panasonic"))
                return Protocol::PanasonicAC;
            if (has("mitsubishi"))
                return Protocol::MitsubishiAC;
            if (has("toshiba"))
                return Protocol::ToshibaAC;
            if (has("fujitsu"))
                return Protocol::FujitsuAC;

            // Fallback: prefer protocolHint exact matches
            if (!hint.empty())
            {
                if (hint == "daikin")
                    return Protocol::DaikinAC;
                if (hint == "panasonic")
                    return Protocol::PanasonicAC;
                if (hint == "mitsubishi")
                    return Protocol::MitsubishiAC;
                if (hint == "toshiba")
                    return Protocol::ToshibaAC;
                if (hint == "fujitsu")
                    return Protocol::FujitsuAC;
            }

            // Default to DaikinAC placeholder if nothing matches
            return Protocol::DaikinAC;
        }
    } // namespace

    bool decodeAC(const esp32ir::RxResult &in, const esp32ir::ac::Capabilities &capabilities, esp32ir::ac::DeviceState &out)
    {
        Protocol proto = protocolFromCapabilities(capabilities);
        switch (proto)
        {
        case Protocol::DaikinAC:
            return decodeDaikinAC(in, capabilities, out);
        case Protocol::PanasonicAC:
            return decodePanasonicAC(in, capabilities, out);
        case Protocol::MitsubishiAC:
            return decodeMitsubishiAC(in, capabilities, out);
        case Protocol::ToshibaAC:
            return decodeToshibaAC(in, capabilities, out);
        case Protocol::FujitsuAC:
            return decodeFujitsuAC(in, capabilities, out);
        default:
            ESP_LOGW("ESP32IRPulseCodec", "decodeAC: unsupported AC protocol");
            return false;
        }
    }

    bool Transmitter::sendAC(const esp32ir::ac::DeviceState &state, const esp32ir::ac::Capabilities &capabilities)
    {
        Protocol proto = protocolFromCapabilities(capabilities);
        switch (proto)
        {
        case Protocol::DaikinAC:
            return sendDaikinAC(state, capabilities);
        case Protocol::PanasonicAC:
            return sendPanasonicAC(state, capabilities);
        case Protocol::MitsubishiAC:
            return sendMitsubishiAC(state, capabilities);
        case Protocol::ToshibaAC:
            return sendToshibaAC(state, capabilities);
        case Protocol::FujitsuAC:
            return sendFujitsuAC(state, capabilities);
        default:
            ESP_LOGW("ESP32IRPulseCodec", "sendAC: unsupported AC protocol");
            return false;
        }
    }

} // namespace esp32ir
