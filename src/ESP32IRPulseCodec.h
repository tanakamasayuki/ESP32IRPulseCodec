#ifndef ESP32IRPULSECODEC_H
#define ESP32IRPULSECODEC_H

#include <stddef.h>
#include <stdint.h>
#include <vector>
#ifdef ESP_PLATFORM
#include <driver/rmt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#endif

#define ESP32IRPULSECODEC_VERSION_MAJOR 0
#define ESP32IRPULSECODEC_VERSION_MINOR 0
#define ESP32IRPULSECODEC_VERSION_PATCH 1

namespace esp32ir
{

  // Protocol identifiers
  enum class Protocol : uint16_t
  {
    RAW = 0,
    NEC,
    SONY,
    AEHA,
    Panasonic,
    JVC,
    Samsung,
    LG,
    Denon,
    RC5,
    RC6,
    Apple,
    Pioneer,
    Toshiba,
    Mitsubishi,
    Hitachi,
    DaikinAC,
    PanasonicAC,
    MitsubishiAC,
    ToshibaAC,
    FujitsuAC,
  };

  // Rx status
  enum class RxStatus : uint8_t
  {
    DECODED = 0,
    RAW_ONLY,
    OVERFLOW,
  };

  // ITPS core types
  struct ITPSFrame
  {
    uint16_t T_us;
    uint16_t len;
    const int8_t *seq;
    uint8_t flags;
  };

  class ITPSBuffer
  {
  public:
    ITPSBuffer() = default;

    void clear();
    void addFrame(const esp32ir::ITPSFrame &f);

    uint16_t frameCount() const;
    const esp32ir::ITPSFrame &frame(uint16_t i) const;
    uint32_t totalTimeUs() const;

  private:
    struct FrameStorage
    {
      esp32ir::ITPSFrame frame;
      std::vector<int8_t> data;
    };
    std::vector<FrameStorage> frames_;
  };

  struct ProtocolMessage
  {
    esp32ir::Protocol protocol;
    const uint8_t *data;
    uint16_t length;
    uint32_t flags;
  };

  struct RxResult
  {
    esp32ir::RxStatus status;
    esp32ir::Protocol protocol;
    esp32ir::ProtocolMessage message;
    esp32ir::ITPSBuffer raw;
    std::vector<uint8_t> payloadStorage;
  };

  namespace payload
  {
    struct NEC
    {
      uint16_t address;
      uint8_t command;
      bool repeat;
    };

    struct SONY
    {
      uint16_t address;
      uint16_t command;
      uint8_t bits;
    };

    struct AEHA
    {
      uint16_t address;
      uint32_t data;
      uint8_t nbits;
    };

    struct Panasonic
    {
      uint16_t address;
      uint32_t data;
      uint8_t nbits;
    };

    struct JVC
    {
      uint16_t address;
      uint16_t command;
    };

    struct Samsung
    {
      uint16_t address;
      uint16_t command;
    };

    struct LG
    {
      uint16_t address;
      uint16_t command;
    };

    struct Denon
    {
      uint16_t address;
      uint16_t command;
      bool repeat;
    };

    struct RC5
    {
      uint16_t command;
      bool toggle;
    };

    struct RC6
    {
      uint32_t command;
      uint8_t mode;
      bool toggle;
    };

    struct Apple
    {
      uint16_t address;
      uint8_t command;
    };

    struct Pioneer
    {
      uint16_t address;
      uint16_t command;
      uint8_t extra;
    };

    struct Toshiba
    {
      uint16_t address;
      uint16_t command;
      uint8_t extra;
    };

    struct Mitsubishi
    {
      uint16_t address;
      uint16_t command;
      uint8_t extra;
    };

    struct Hitachi
    {
      uint16_t address;
      uint16_t command;
      uint8_t extra;
    };

    // AC payload structs are reserved (fields TBD)
    struct DaikinAC
    {
    };
    struct PanasonicAC
    {
    };
    struct MitsubishiAC
    {
    };
    struct ToshibaAC
    {
    };
    struct FujitsuAC
    {
    };

  } // namespace payload

  // Receiver
  class Receiver
  {
  public:
    Receiver();
    Receiver(int rxPin, bool invert = false, uint16_t T_us_rx = 5);

    bool setPin(int rxPin);
    bool setInvertInput(bool invert);
    bool setQuantizeT(uint16_t T_us_rx);

    bool begin();
    void end();

    bool addProtocol(esp32ir::Protocol protocol);
    bool clearProtocols();
    bool useRawOnly();
    bool useRawPlusKnown();
    bool useKnownWithoutAC();

    bool poll(esp32ir::RxResult &out);

  private:
    int rxPin_{-1};
    bool invertInput_{false};
    uint16_t quantizeT_{5};
    bool useRawOnly_{false};
    bool useRawPlusKnown_{false};
    bool useKnownNoAC_{false};
    bool begun_{false};
    std::vector<esp32ir::Protocol> protocols_;
#ifdef ESP_PLATFORM
    int rmtChannel_{0};
    RingbufHandle_t rmtRingbuf_{nullptr};
#endif
  };

  // Transmitter
  class Transmitter
  {
  public:
    Transmitter();
    Transmitter(int txPin, bool invert = false, uint32_t hz = 38000);

    bool setPin(int txPin);
    bool setInvertOutput(bool invert);
    bool setCarrierHz(uint32_t hz);
    bool setDutyPercent(uint8_t dutyPercent);
    bool setGapUs(uint32_t gapUs);

    bool begin();
    void end();

    bool send(const esp32ir::ITPSBuffer &itps);
    bool send(const esp32ir::ProtocolMessage &message);

    // Protocol-specific send helpers (struct + args where applicable; AC is struct only)
    bool sendNEC(const esp32ir::payload::NEC &p);
    bool sendNEC(uint16_t address, uint8_t command, bool repeat = false);
    bool sendSONY(const esp32ir::payload::SONY &p);
    bool sendSONY(uint16_t address, uint16_t command, uint8_t bits = 12);
    bool sendAEHA(const esp32ir::payload::AEHA &p);
    bool sendAEHA(uint16_t address, uint32_t data, uint8_t nbits);
    bool sendPanasonic(const esp32ir::payload::Panasonic &p);
    bool sendPanasonic(uint16_t address, uint32_t data, uint8_t nbits);
    bool sendJVC(const esp32ir::payload::JVC &p);
    bool sendJVC(uint16_t address, uint16_t command);
    bool sendSamsung(const esp32ir::payload::Samsung &p);
    bool sendSamsung(uint16_t address, uint16_t command);
    bool sendLG(const esp32ir::payload::LG &p);
    bool sendLG(uint16_t address, uint16_t command);
    bool sendDenon(const esp32ir::payload::Denon &p);
    bool sendDenon(uint16_t address, uint16_t command, bool repeat = false);
    bool sendRC5(const esp32ir::payload::RC5 &p);
    bool sendRC5(uint16_t command, bool toggle);
    bool sendRC6(const esp32ir::payload::RC6 &p);
    bool sendRC6(uint32_t command, uint8_t mode, bool toggle);
    bool sendApple(const esp32ir::payload::Apple &p);
    bool sendApple(uint16_t address, uint8_t command);
    bool sendPioneer(const esp32ir::payload::Pioneer &p);
    bool sendPioneer(uint16_t address, uint16_t command, uint8_t extra = 0);
    bool sendToshiba(const esp32ir::payload::Toshiba &p);
    bool sendToshiba(uint16_t address, uint16_t command, uint8_t extra = 0);
    bool sendMitsubishi(const esp32ir::payload::Mitsubishi &p);
    bool sendMitsubishi(uint16_t address, uint16_t command, uint8_t extra = 0);
    bool sendHitachi(const esp32ir::payload::Hitachi &p);
    bool sendHitachi(uint16_t address, uint16_t command, uint8_t extra = 0);
    bool sendDaikinAC(const esp32ir::payload::DaikinAC &p);
    bool sendPanasonicAC(const esp32ir::payload::PanasonicAC &p);
    bool sendMitsubishiAC(const esp32ir::payload::MitsubishiAC &p);
    bool sendToshibaAC(const esp32ir::payload::ToshibaAC &p);
    bool sendFujitsuAC(const esp32ir::payload::FujitsuAC &p);

  private:
    int txPin_{-1};
    bool invertOutput_{false};
    uint32_t carrierHz_{38000};
    uint8_t dutyPercent_{50};
    uint32_t gapUs_{40000};
    bool begun_{false};
#ifdef ESP_PLATFORM
    int txChannel_{1};
#endif
  };

  // Decode helpers
  bool decodeNEC(const esp32ir::RxResult &in, esp32ir::payload::NEC &out);
  bool decodeSONY(const esp32ir::RxResult &in, esp32ir::payload::SONY &out);
  bool decodeAEHA(const esp32ir::RxResult &in, esp32ir::payload::AEHA &out);
  bool decodePanasonic(const esp32ir::RxResult &in, esp32ir::payload::Panasonic &out);
  bool decodeJVC(const esp32ir::RxResult &in, esp32ir::payload::JVC &out);
  bool decodeSamsung(const esp32ir::RxResult &in, esp32ir::payload::Samsung &out);
  bool decodeLG(const esp32ir::RxResult &in, esp32ir::payload::LG &out);
  bool decodeDenon(const esp32ir::RxResult &in, esp32ir::payload::Denon &out);
  bool decodeRC5(const esp32ir::RxResult &in, esp32ir::payload::RC5 &out);
  bool decodeRC6(const esp32ir::RxResult &in, esp32ir::payload::RC6 &out);
  bool decodeApple(const esp32ir::RxResult &in, esp32ir::payload::Apple &out);
  bool decodePioneer(const esp32ir::RxResult &in, esp32ir::payload::Pioneer &out);
  bool decodeToshiba(const esp32ir::RxResult &in, esp32ir::payload::Toshiba &out);
  bool decodeMitsubishi(const esp32ir::RxResult &in, esp32ir::payload::Mitsubishi &out);
  bool decodeHitachi(const esp32ir::RxResult &in, esp32ir::payload::Hitachi &out);
  bool decodeDaikinAC(const esp32ir::RxResult &in, esp32ir::payload::DaikinAC &out);
  bool decodePanasonicAC(const esp32ir::RxResult &in, esp32ir::payload::PanasonicAC &out);
  bool decodeMitsubishiAC(const esp32ir::RxResult &in, esp32ir::payload::MitsubishiAC &out);
  bool decodeToshibaAC(const esp32ir::RxResult &in, esp32ir::payload::ToshibaAC &out);
  bool decodeFujitsuAC(const esp32ir::RxResult &in, esp32ir::payload::FujitsuAC &out);

} // namespace esp32ir

#endif // ESP32IRPULSECODEC_H
