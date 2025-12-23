#include "ESP32IRPulseCodec.h"
#include <cstring>
#include <vector>

namespace esp32ir
{
    // Forward declarations for per-protocol builders
    bool buildNECTxBitstream(uint16_t address, uint8_t command, std::vector<uint8_t> &outBytes);
    bool buildTxBitstream(const esp32ir::ProtocolMessage &message, std::vector<uint8_t> &out, uint16_t &bitCount)
    {
        out.clear();
        bitCount = 0;
        uint16_t bitIndex = 0;
        auto addBit = [&](bool one)
        {
            size_t byteIndex = bitIndex / 8;
            if (out.size() <= byteIndex)
                out.push_back(0);
            if (one)
                out[byteIndex] |= static_cast<uint8_t>(1u << (bitIndex % 8));
            ++bitIndex;
        };
        switch (message.protocol)
        {
        case esp32ir::Protocol::NEC:
        {
            if (message.length != sizeof(esp32ir::payload::NEC) || message.data == nullptr)
                return false;
            esp32ir::payload::NEC p{};
            std::memcpy(&p, message.data, sizeof(p));
            // Reuse NEC helper in protocols
            extern bool buildNECTxBitstream(uint16_t address, uint8_t command, std::vector<uint8_t> &outBytes);
            bool ok = buildNECTxBitstream(p.address, p.command, out);
            bitCount = ok ? 32 : 0;
            return ok;
        }
        case esp32ir::Protocol::SONY:
        {
            if (message.length != sizeof(esp32ir::payload::SONY) || message.data == nullptr)
                return false;
            esp32ir::payload::SONY p{};
            std::memcpy(&p, message.data, sizeof(p));
            if (p.bits != 12 && p.bits != 15 && p.bits != 20)
                return false;
            uint32_t payload = static_cast<uint32_t>(p.command) | (static_cast<uint32_t>(p.address) << 7);
            bitIndex = 0;
            for (uint8_t i = 0; i < p.bits; ++i)
            {
                addBit((payload >> i) & 0x1);
            }
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::AEHA:
        {
            if (message.length != sizeof(esp32ir::payload::AEHA) || message.data == nullptr)
                return false;
            esp32ir::payload::AEHA p{};
            std::memcpy(&p, message.data, sizeof(p));
            if (p.nbits == 0 || p.nbits > 32)
                return false;
            uint64_t payload = static_cast<uint64_t>(p.address) | (static_cast<uint64_t>(p.data) << 16);
            bitIndex = 0;
            uint8_t totalBits = static_cast<uint8_t>(16 + p.nbits);
            for (uint8_t i = 0; i < totalBits; ++i)
            {
                addBit((payload >> i) & 0x1);
            }
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::Panasonic:
        {
            if (message.length != sizeof(esp32ir::payload::Panasonic) || message.data == nullptr)
                return false;
            esp32ir::payload::Panasonic p{};
            std::memcpy(&p, message.data, sizeof(p));
            if (p.nbits == 0 || p.nbits > 32)
                return false;
            uint64_t payload = static_cast<uint64_t>(p.address) | (static_cast<uint64_t>(p.data) << 16);
            uint8_t totalBits = static_cast<uint8_t>(16 + p.nbits);
            bitIndex = 0;
            for (uint8_t i = 0; i < totalBits; ++i)
            {
                addBit((payload >> i) & 0x1);
            }
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::JVC:
        {
            if (message.length != sizeof(esp32ir::payload::JVC) || message.data == nullptr)
                return false;
            esp32ir::payload::JVC p{};
            std::memcpy(&p, message.data, sizeof(p));
            uint64_t payload = static_cast<uint64_t>(p.address) | (static_cast<uint64_t>(p.command) << 16);
            bitIndex = 0;
            constexpr uint8_t kBits = 32;
            for (uint8_t i = 0; i < kBits; ++i)
                addBit((payload >> i) & 0x1);
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::Samsung:
        {
            if (message.length != sizeof(esp32ir::payload::Samsung) || message.data == nullptr)
                return false;
            esp32ir::payload::Samsung p{};
            std::memcpy(&p, message.data, sizeof(p));
            uint64_t payload = static_cast<uint64_t>(p.address) | (static_cast<uint64_t>(p.command) << 16);
            bitIndex = 0;
            constexpr uint8_t kBits = 32;
            for (uint8_t i = 0; i < kBits; ++i)
                addBit((payload >> i) & 0x1);
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::LG:
        {
            if (message.length != sizeof(esp32ir::payload::LG) || message.data == nullptr)
                return false;
            esp32ir::payload::LG p{};
            std::memcpy(&p, message.data, sizeof(p));
            uint64_t payload = static_cast<uint64_t>(p.address) | (static_cast<uint64_t>(p.command) << 16);
            bitIndex = 0;
            constexpr uint8_t kBits = 32;
            for (uint8_t i = 0; i < kBits; ++i)
                addBit((payload >> i) & 0x1);
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::Denon:
        {
            if (message.length != sizeof(esp32ir::payload::Denon) || message.data == nullptr)
                return false;
            esp32ir::payload::Denon p{};
            std::memcpy(&p, message.data, sizeof(p));
            uint64_t payload = static_cast<uint64_t>(p.address) | (static_cast<uint64_t>(p.command) << 16);
            bitIndex = 0;
            constexpr uint8_t kBits = 32;
            for (uint8_t i = 0; i < kBits; ++i)
                addBit((payload >> i) & 0x1);
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::RC5:
        {
            if (message.length != sizeof(esp32ir::payload::RC5) || message.data == nullptr)
                return false;
            esp32ir::payload::RC5 p{};
            std::memcpy(&p, message.data, sizeof(p));
            bitIndex = 0;
            // start bits 1,1
            addBit(true);
            addBit(true);
            addBit(p.toggle);
            uint8_t address = 0; // current implementation uses fixed address=0
            for (int i = 4; i >= 0; --i)
                addBit((address >> i) & 0x1);
            uint16_t cmd = p.command & 0x3F;
            for (int i = 5; i >= 0; --i)
                addBit((cmd >> i) & 0x1);
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::RC6:
        {
            if (message.length != sizeof(esp32ir::payload::RC6) || message.data == nullptr)
                return false;
            esp32ir::payload::RC6 p{};
            std::memcpy(&p, message.data, sizeof(p));
            bitIndex = 0;
            addBit(true); // start bit
            uint32_t mode = p.mode & 0x7;
            for (int i = 2; i >= 0; --i)
                addBit((mode >> i) & 0x1);
            addBit(p.toggle);
            uint32_t cmd = p.command & 0xFFFF;
            for (int i = 15; i >= 0; --i)
                addBit((cmd >> i) & 0x1);
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::Apple:
        {
            if (message.length != sizeof(esp32ir::payload::Apple) || message.data == nullptr)
                return false;
            esp32ir::payload::Apple p{};
            std::memcpy(&p, message.data, sizeof(p));
            uint64_t payload = static_cast<uint64_t>(p.address) |
                               (static_cast<uint64_t>(p.command) << 16) |
                               (static_cast<uint64_t>(~p.command & 0xFF) << 24);
            bitIndex = 0;
            constexpr uint8_t kBits = 32;
            for (uint8_t i = 0; i < kBits; ++i)
                addBit((payload >> i) & 0x1);
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::Pioneer:
        {
            if (message.length != sizeof(esp32ir::payload::Pioneer) || message.data == nullptr)
                return false;
            esp32ir::payload::Pioneer p{};
            std::memcpy(&p, message.data, sizeof(p));
            uint64_t payload = static_cast<uint64_t>(p.address) |
                               (static_cast<uint64_t>(p.command) << 16) |
                               (static_cast<uint64_t>(p.extra) << 32);
            bitIndex = 0;
            constexpr uint8_t kBits = 40;
            for (uint8_t i = 0; i < kBits; ++i)
                addBit((payload >> i) & 0x1);
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::Toshiba:
        {
            if (message.length != sizeof(esp32ir::payload::Toshiba) || message.data == nullptr)
                return false;
            esp32ir::payload::Toshiba p{};
            std::memcpy(&p, message.data, sizeof(p));
            uint64_t payload = static_cast<uint64_t>(p.address) |
                               (static_cast<uint64_t>(p.command) << 16) |
                               (static_cast<uint64_t>(p.extra) << 32);
            bitIndex = 0;
            constexpr uint8_t kBits = 40;
            for (uint8_t i = 0; i < kBits; ++i)
                addBit((payload >> i) & 0x1);
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::Mitsubishi:
        {
            if (message.length != sizeof(esp32ir::payload::Mitsubishi) || message.data == nullptr)
                return false;
            esp32ir::payload::Mitsubishi p{};
            std::memcpy(&p, message.data, sizeof(p));
            uint64_t payload = static_cast<uint64_t>(p.address) |
                               (static_cast<uint64_t>(p.command) << 16) |
                               (static_cast<uint64_t>(p.extra) << 32);
            bitIndex = 0;
            constexpr uint8_t kBits = 40;
            for (uint8_t i = 0; i < kBits; ++i)
                addBit((payload >> i) & 0x1);
            bitCount = bitIndex;
            return true;
        }
        case esp32ir::Protocol::Hitachi:
        {
            if (message.length != sizeof(esp32ir::payload::Hitachi) || message.data == nullptr)
                return false;
            esp32ir::payload::Hitachi p{};
            std::memcpy(&p, message.data, sizeof(p));
            uint64_t payload = static_cast<uint64_t>(p.address) |
                               (static_cast<uint64_t>(p.command) << 16) |
                               (static_cast<uint64_t>(p.extra) << 32);
            bitIndex = 0;
            constexpr uint8_t kBits = 40;
            for (uint8_t i = 0; i < kBits; ++i)
                addBit((payload >> i) & 0x1);
            bitCount = bitIndex;
            return true;
        }
        default:
            // For unsupported protocols, fall back to raw bytes (if any)
            if (message.data && message.length)
            {
                const uint8_t *p = static_cast<const uint8_t *>(message.data);
                out.assign(p, p + message.length);
                bitCount = static_cast<uint16_t>(message.length * 8);
                return true;
            }
            return false;
        }
    }
} // namespace esp32ir
