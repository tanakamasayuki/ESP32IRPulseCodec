#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

  uint16_t ITPSBuffer::frameCount() const { return 0; }

  const esp32ir::ITPSFrame &ITPSBuffer::frame(uint16_t) const
  {
    static const esp32ir::ITPSFrame kEmptyFrame{0, 0, nullptr, 0};
    return kEmptyFrame;
  }

  uint32_t ITPSBuffer::totalTimeUs() const { return 0; }

} // namespace esp32ir
