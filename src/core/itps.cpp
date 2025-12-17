#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

  void ITPSBuffer::clear() { frames_.clear(); }

  void ITPSBuffer::addFrame(const esp32ir::ITPSFrame &f) { frames_.push_back(f); }

  uint16_t ITPSBuffer::frameCount() const { return static_cast<uint16_t>(frames_.size()); }

  const esp32ir::ITPSFrame &ITPSBuffer::frame(uint16_t i) const
  {
    static const esp32ir::ITPSFrame kEmptyFrame{0, 0, nullptr, 0};
    if (i < frames_.size())
    {
      return frames_[i];
    }
    return kEmptyFrame;
  }

  uint32_t ITPSBuffer::totalTimeUs() const
  {
    uint32_t total = 0;
    for (const auto &f : frames_)
    {
      if (!f.seq || f.len == 0 || f.T_us == 0)
      {
        continue;
      }
      for (uint16_t i = 0; i < f.len; ++i)
      {
        int v = f.seq[i];
        uint32_t mag = (v < 0) ? static_cast<uint32_t>(-v) : static_cast<uint32_t>(v);
        total += mag * static_cast<uint32_t>(f.T_us);
      }
    }
    return total;
  }

} // namespace esp32ir
