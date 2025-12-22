#ifndef ESP32IRPULSECODEC_H
#define ESP32IRPULSECODEC_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#ifndef ESP_PLATFORM
#error "ESP32IRPulseCodec is intended for ESP32 (ESP_PLATFORM must be defined)."
#endif

#include "esp32irpulsecodec_version.h"
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <unordered_map>
#include <driver/rmt_tx.h>
#include <driver/rmt_rx.h>
#include <driver/rmt_encoder.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

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

  enum class RxSplitPolicy : uint8_t
  {
    DROP_GAP = 0,
    KEEP_GAP_IN_FRAME,
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

  } // namespace payload

  namespace ac
  {
    enum class TemperatureUnit
    {
      C,
      F
    };

    enum class FeatureValueType
    {
      Boolean,
      Number,
      String,
      OnOffToggle
    };

    enum class OutOfRangeTemperaturePolicy
    {
      Reject,
      Coerce
    };

    enum class UnsupportedModePolicy
    {
      Reject,
      CoerceToAuto,
      CoerceToOff
    };

    enum class UnsupportedFanSpeedPolicy
    {
      Reject,
      CoerceToAuto,
      CoerceToNearest
    };

    enum class UnsupportedSwingPolicy
    {
      Reject,
      CoerceToOff,
      CoerceToNearest
    };

    using FeatureValue = std::variant<bool, double, std::string, std::nullptr_t>;

    struct DeviceRef
    {
      std::string vendor;
      std::string model;
      std::optional<std::string> brand;
      std::optional<std::string> protocolHint;
      std::optional<std::string> notes;
    };

    struct FanSpeed
    {
      bool isAuto{true};
      int level{0}; // valid only if !isAuto
    };

    struct Swing
    {
      std::optional<std::string> vertical;
      std::optional<std::string> horizontal;
    };

    struct Opaque
    {
      std::vector<uint8_t> preservedBits;
      std::optional<std::string> vendorData;
    };

    struct DeviceState
    {
      std::optional<DeviceRef> device;
      bool power{false};
      std::string mode;
      double targetTemperature{0};
      TemperatureUnit temperatureUnit{TemperatureUnit::C};
      FanSpeed fanSpeed{};
      Swing swing{};
      std::unordered_map<std::string, FeatureValue> features;
      std::optional<int> sleepTimerMinutes;
      std::optional<int> clockMinutesSinceMidnight;
      std::optional<Opaque> opaque;
    };

    struct TemperatureCaps
    {
      double min{0};
      double max{0};
      double step{1};
      std::vector<std::string> units;
    };

    struct FanCaps
    {
      bool autoSupported{true};
      int levels{0};
      std::vector<std::string> namedLevels;
    };

    struct SwingCaps
    {
      std::vector<std::string> verticalValues;
      std::vector<std::string> horizontalValues;
    };

    struct FeatureCaps
    {
      std::vector<std::string> supported;
      std::vector<std::string> toggleOnly;
      std::vector<std::vector<std::string>> mutuallyExclusiveSets;
      std::unordered_map<std::string, FeatureValueType> featureValueTypes;
    };

    struct PowerBehavior
    {
      bool powerRequiredForModeChange{true};
      bool powerRequiredForTempChange{true};
    };

    struct ValidationPolicy
    {
      OutOfRangeTemperaturePolicy outOfRangeTemperature{OutOfRangeTemperaturePolicy::Reject};
      UnsupportedModePolicy unsupportedMode{UnsupportedModePolicy::Reject};
      UnsupportedFanSpeedPolicy unsupportedFanSpeed{UnsupportedFanSpeedPolicy::Reject};
      UnsupportedSwingPolicy unsupportedSwing{UnsupportedSwingPolicy::Reject};
    };

    struct Capabilities
    {
      DeviceRef device;
      TemperatureCaps temperature;
      std::vector<std::string> modes;
      FanCaps fan;
      SwingCaps swing;
      FeatureCaps features;
      PowerBehavior powerBehavior;
      ValidationPolicy validationPolicy;
    };

  } // namespace ac

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
    bool setFrameGapUs(uint32_t frameGapUs);
    bool setHardGapUs(uint32_t hardGapUs);
    bool setMinFrameUs(uint32_t minFrameUs);
    bool setMaxFrameUs(uint32_t maxFrameUs);
    bool setMinEdges(uint16_t minEdges);
    bool setFrameCountMax(uint16_t frameCountMax);
    bool setSplitPolicy(RxSplitPolicy policy);

    bool poll(esp32ir::RxResult &out);

    struct RxCallbackContext
    {
      QueueHandle_t queue;
      volatile bool *overflowFlag;
    };

  private:
    int rxPin_{-1};
    bool invertInput_{false};
    uint16_t quantizeT_{5};
    bool useRawOnly_{false};
    bool useRawPlusKnown_{false};
    bool useKnownNoAC_{false};
    uint32_t frameGapUs_{0};
    uint32_t hardGapUs_{0};
    uint32_t minFrameUs_{0};
    uint32_t maxFrameUs_{0};
    uint16_t minEdges_{0};
    uint16_t frameCountMax_{0};
    RxSplitPolicy splitPolicy_{RxSplitPolicy::DROP_GAP};
    bool splitPolicySet_{false};
    // Effective params resolved at begin (spec: merge defaults/recommendations at begin)
    uint32_t effFrameGapUs_{0};
    uint32_t effHardGapUs_{0};
    uint32_t effMinFrameUs_{0};
    uint32_t effMaxFrameUs_{0};
    uint16_t effMinEdges_{0};
    uint16_t effFrameCountMax_{0};
    RxSplitPolicy effSplitPolicy_{RxSplitPolicy::DROP_GAP};
    bool begun_{false};
    std::vector<esp32ir::Protocol> protocols_;
    rmt_channel_handle_t rxChannel_{nullptr};
    QueueHandle_t rxQueue_{nullptr};
    std::vector<rmt_symbol_word_t> rxBuffer_;
    rmt_receive_config_t rxConfig_{};
    RxCallbackContext rxCallbackCtx_{};
    volatile bool rxOverflowed_{false};
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
    // AC common API + brand-specific (common types)
    bool sendAC(const esp32ir::ac::DeviceState &state, const esp32ir::ac::Capabilities &capabilities);
    bool sendDaikinAC(const esp32ir::ac::DeviceState &state, const esp32ir::ac::Capabilities &capabilities);
    bool sendPanasonicAC(const esp32ir::ac::DeviceState &state, const esp32ir::ac::Capabilities &capabilities);
    bool sendMitsubishiAC(const esp32ir::ac::DeviceState &state, const esp32ir::ac::Capabilities &capabilities);
    bool sendToshibaAC(const esp32ir::ac::DeviceState &state, const esp32ir::ac::Capabilities &capabilities);
    bool sendFujitsuAC(const esp32ir::ac::DeviceState &state, const esp32ir::ac::Capabilities &capabilities);

  private:
    int txPin_{-1};
    bool invertOutput_{false};
    uint32_t carrierHz_{38000};
    uint8_t dutyPercent_{50};
    uint32_t gapUs_{40000};
    bool gapOverridden_{false};
    bool begun_{false};
    rmt_channel_handle_t txChannel_{nullptr};
    rmt_encoder_handle_t txEncoder_{nullptr};

    bool sendWithGap(const esp32ir::ITPSBuffer &itps, uint32_t recommendedGapUs);
    uint32_t recommendedGapUs(esp32ir::Protocol proto) const;
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
  // AC common API
  bool decodeAC(const esp32ir::RxResult &in, const esp32ir::ac::Capabilities &capabilities, esp32ir::ac::DeviceState &out);
  bool decodeDaikinAC(const esp32ir::RxResult &in, const esp32ir::ac::Capabilities &capabilities, esp32ir::ac::DeviceState &out);
  bool decodePanasonicAC(const esp32ir::RxResult &in, const esp32ir::ac::Capabilities &capabilities, esp32ir::ac::DeviceState &out);
  bool decodeMitsubishiAC(const esp32ir::RxResult &in, const esp32ir::ac::Capabilities &capabilities, esp32ir::ac::DeviceState &out);
  bool decodeToshibaAC(const esp32ir::RxResult &in, const esp32ir::ac::Capabilities &capabilities, esp32ir::ac::DeviceState &out);
  bool decodeFujitsuAC(const esp32ir::RxResult &in, const esp32ir::ac::Capabilities &capabilities, esp32ir::ac::DeviceState &out);

} // namespace esp32ir

#endif // ESP32IRPULSECODEC_H
