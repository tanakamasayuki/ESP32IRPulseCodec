#include "ESP32IRPulseCodec.h"
#include <driver/rmt_rx.h>
#include <driver/rmt_types.h>
#include <algorithm>

namespace esp32ir
{

    namespace
    {
        constexpr const char *kTag = "ESP32IRPulseCodec";
        constexpr uint32_t kRmtRefClockHz = 1000000; // REF_TICK
        constexpr uint32_t kRmtMaxDivider = 256;
        constexpr uint32_t kRmtMinResolutionHz = (kRmtRefClockHz + kRmtMaxDivider - 1) / kRmtMaxDivider;

        const char *splitPolicyName(esp32ir::RxSplitPolicy policy)
        {
            switch (policy)
            {
            case esp32ir::RxSplitPolicy::DROP_GAP:
                return "DROP_GAP";
            case esp32ir::RxSplitPolicy::KEEP_GAP_IN_FRAME:
                return "KEEP_GAP_IN_FRAME";
            default:
                return "UNKNOWN";
            }
        }

        bool rxDoneCallback(rmt_channel_handle_t, const rmt_rx_done_event_data_t *edata, void *user_ctx)
        {
            auto ctx = static_cast<esp32ir::Receiver::RxCallbackContext *>(user_ctx);
            if (!ctx || !ctx->queue || !ctx->overflowFlag || !edata)
            {
                return false;
            }
            BaseType_t high_task_woken = pdFALSE;
            uint8_t bufIdx = 0xFF;
            for (uint8_t i = 0; i < 2; ++i)
            {
                if (ctx->buffers[i] == edata->received_symbols)
                {
                    bufIdx = i;
                    break;
                }
            }
            if (bufIdx > 1)
            {
                *(ctx->overflowFlag) = true;
            }
            else
            {
                uint8_t mask = *(ctx->pendingMask);
                mask |= static_cast<uint8_t>(1u << bufIdx);
                *(ctx->pendingMask) = mask;
            }
            if (xQueueSendFromISR(ctx->queue, edata, &high_task_woken) != pdTRUE)
            {
                // Drop oldest to make room (spec: older entries are dropped on overflow).
                rmt_rx_done_event_data_t dummy{};
                xQueueReceiveFromISR(ctx->queue, &dummy, &high_task_woken);
                if (xQueueSendFromISR(ctx->queue, edata, &high_task_woken) != pdTRUE)
                {
                    *(ctx->overflowFlag) = true;
                }
                else
                {
                    *(ctx->overflowFlag) = true;
                }
            }
            if (!edata->flags.is_last)
            {
                *(ctx->overflowFlag) = true;
            }
            // Attempt to re-arm reception using a free buffer
            int freeIdx = -1;
            uint8_t mask = *(ctx->pendingMask);
            for (uint8_t i = 0; i < 2; ++i)
            {
                if ((mask & (1u << i)) == 0 && ctx->buffers[i])
                {
                    freeIdx = static_cast<int>(i);
                    break;
                }
            }
            if (freeIdx >= 0 && ctx->channel && ctx->rxConfig)
            {
                esp_err_t err = rmt_receive(ctx->channel, ctx->buffers[freeIdx], ctx->bufferLenSymbols * sizeof(rmt_symbol_word_t), ctx->rxConfig);
                if (err != ESP_OK)
                {
                    *(ctx->overflowFlag) = true;
                    if (ctx->needRestart)
                        *(ctx->needRestart) = true;
                }
                else if (ctx->needRestart)
                {
                    *(ctx->needRestart) = false;
                }
            }
            else if (ctx->needRestart)
            {
                *(ctx->needRestart) = true;
            }
            return high_task_woken == pdTRUE;
        }

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

        struct RxParams
        {
            uint32_t frameGapUs;
            uint32_t hardGapUs;
            uint32_t minFrameUs;
            uint32_t maxFrameUs;
            uint16_t minEdges;
            uint16_t frameCountMax;
            esp32ir::RxSplitPolicy splitPolicy;
        };

        RxParams defaultParams(bool rawMode)
        {
            // Conservative defaults; can be refined per-protocol later.
            RxParams p{};
            p.frameGapUs = 10000;
            p.hardGapUs = 20000;
            p.minFrameUs = 500;
            p.maxFrameUs = 200000;
            p.minEdges = 4;
            p.frameCountMax = 8;
            p.splitPolicy = rawMode ? esp32ir::RxSplitPolicy::KEEP_GAP_IN_FRAME : esp32ir::RxSplitPolicy::DROP_GAP;
            return p;
        }

        void mergeParams(RxParams &base, const RxParams &rec)
        {
            if (rec.frameGapUs > 0)
                base.frameGapUs = std::max(base.frameGapUs, rec.frameGapUs);
            if (rec.hardGapUs > 0)
                base.hardGapUs = std::max(base.hardGapUs, rec.hardGapUs);
            if (rec.maxFrameUs > 0)
                base.maxFrameUs = std::max(base.maxFrameUs, rec.maxFrameUs);
            if (rec.minFrameUs > 0)
                base.minFrameUs = std::max(base.minFrameUs, rec.minFrameUs);
            if (rec.minEdges > 0)
                base.minEdges = std::max(base.minEdges, rec.minEdges);
            if (rec.frameCountMax > 0)
                base.frameCountMax = std::max(base.frameCountMax, rec.frameCountMax);
            // splitPolicy is selected later; recommendations do not override user/raw choice.
        }

        RxParams recommendedParamsForProtocol(esp32ir::Protocol p)
        {
            // Heuristic recommendations to help frame splitting; kept permissive to avoid data loss.
            switch (p)
            {
            case esp32ir::Protocol::NEC:
            case esp32ir::Protocol::Samsung:
            case esp32ir::Protocol::Apple:
            {
                const auto &p = esp32ir::proto_const::kNECParams;
                return {p.frameGapUs, p.hardGapUs, p.minFrameUs, p.maxFrameUs, p.minEdges, p.frameCountMax, p.splitPolicy};
            }
            case esp32ir::Protocol::SONY:
            {
                const auto &rp = esp32ir::proto_const::kSonyParams;
                return {rp.frameGapUs, rp.hardGapUs, rp.minFrameUs, rp.maxFrameUs, rp.minEdges, rp.frameCountMax, rp.splitPolicy};
            }
            case esp32ir::Protocol::AEHA:
            {
                const auto &rp = esp32ir::proto_const::kAEHAParams;
                return {rp.frameGapUs, rp.hardGapUs, rp.minFrameUs, rp.maxFrameUs, rp.minEdges, rp.frameCountMax, rp.splitPolicy};
            }
            case esp32ir::Protocol::Panasonic:
            {
                const auto &rp = esp32ir::proto_const::kPanasonicParams;
                return {rp.frameGapUs, rp.hardGapUs, rp.minFrameUs, rp.maxFrameUs, rp.minEdges, rp.frameCountMax, rp.splitPolicy};
            }
            case esp32ir::Protocol::JVC:
            {
                const auto &rp = esp32ir::proto_const::kJVCParams;
                return {rp.frameGapUs, rp.hardGapUs, rp.minFrameUs, rp.maxFrameUs, rp.minEdges, rp.frameCountMax, rp.splitPolicy};
            }
            case esp32ir::Protocol::LG:
            case esp32ir::Protocol::Denon:
            case esp32ir::Protocol::Toshiba:
            case esp32ir::Protocol::Mitsubishi:
            case esp32ir::Protocol::Hitachi:
            case esp32ir::Protocol::Pioneer:
            {
                const auto &rp = esp32ir::proto_const::kLgGroupParams;
                return {rp.frameGapUs, rp.hardGapUs, rp.minFrameUs, rp.maxFrameUs, rp.minEdges, rp.frameCountMax, rp.splitPolicy};
            }
            case esp32ir::Protocol::RC5:
            {
                const auto &rp = esp32ir::proto_const::kRC5Params;
                return {rp.frameGapUs, rp.hardGapUs, rp.minFrameUs, rp.maxFrameUs, rp.minEdges, rp.frameCountMax, rp.splitPolicy};
            }
            case esp32ir::Protocol::RC6:
            {
                const auto &rp = esp32ir::proto_const::kRC6Params;
                return {rp.frameGapUs, rp.hardGapUs, rp.minFrameUs, rp.maxFrameUs, rp.minEdges, rp.frameCountMax, rp.splitPolicy};
            }
            case esp32ir::Protocol::PanasonicAC:
            case esp32ir::Protocol::MitsubishiAC:
            case esp32ir::Protocol::ToshibaAC:
            case esp32ir::Protocol::DaikinAC:
            case esp32ir::Protocol::FujitsuAC:
            {
                const auto &rp = esp32ir::proto_const::kACParams;
                return {rp.frameGapUs, rp.hardGapUs, rp.minFrameUs, rp.maxFrameUs, rp.minEdges, rp.frameCountMax, rp.splitPolicy};
            }
            case esp32ir::Protocol::RAW:
                return defaultParams(true);
            default:
                return RxParams{};
            }
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
    bool Receiver::setFrameGapUs(uint32_t frameGapUs)
    {
        if (begun_)
            return false;
        frameGapUs_ = frameGapUs;
        return true;
    }
    bool Receiver::setHardGapUs(uint32_t hardGapUs)
    {
        if (begun_)
            return false;
        hardGapUs_ = hardGapUs;
        return true;
    }
    bool Receiver::setMinFrameUs(uint32_t minFrameUs)
    {
        if (begun_)
            return false;
        minFrameUs_ = minFrameUs;
        return true;
    }
    bool Receiver::setMaxFrameUs(uint32_t maxFrameUs)
    {
        if (begun_)
            return false;
        maxFrameUs_ = maxFrameUs;
        return true;
    }
    bool Receiver::setMinEdges(uint16_t minEdges)
    {
        if (begun_)
            return false;
        minEdges_ = minEdges;
        return true;
    }
    bool Receiver::setFrameCountMax(uint16_t frameCountMax)
    {
        if (begun_)
            return false;
        frameCountMax_ = frameCountMax;
        return true;
    }
    bool Receiver::setSplitPolicy(RxSplitPolicy policy)
    {
        if (begun_)
            return false;
        splitPolicy_ = policy;
        splitPolicySet_ = true;
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
        rxOverflowed_ = false;
        // Match RMT resolution to quantizeT_ (T_us) to minimize timing error, while keeping divider in range.
        uint32_t resolutionHz = kRmtRefClockHz / std::max<uint16_t>(1, quantizeT_);
        if (resolutionHz < kRmtMinResolutionHz)
        {
            resolutionHz = kRmtMinResolutionHz;
            uint16_t adjustedQuantize = static_cast<uint16_t>((kRmtRefClockHz + resolutionHz - 1) / resolutionHz);
            if (adjustedQuantize == 0)
                adjustedQuantize = 1;
            if (adjustedQuantize != quantizeT_)
            {
                ESP_LOGW(kTag, "RX quantizeT adjusted to %u us to satisfy RMT divider", static_cast<unsigned>(adjustedQuantize));
            }
            quantizeT_ = adjustedQuantize;
        }
        rmt_rx_channel_config_t config = {
            .gpio_num = static_cast<gpio_num_t>(rxPin_),
            .clk_src = RMT_CLK_SRC_REF_TICK,
            .resolution_hz = resolutionHz,
            .mem_block_symbols = 64,
            .intr_priority = 0,
            .flags = {
                .invert_in = invertInput_ ? 1U : 0U,
                .with_dma = 0,
                .io_loop_back = 0,
                .allow_pd = 0,
            },
        };
        if (rmt_new_rx_channel(&config, &rxChannel_) != ESP_OK)
        {
            ESP_LOGE(kTag, "RX begin failed: rmt_new_rx_channel");
            return false;
        }
        if (rmt_enable(rxChannel_) != ESP_OK)
        {
            ESP_LOGE(kTag, "RX begin failed: rmt_enable");
            rmt_del_channel(rxChannel_);
            rxChannel_ = nullptr;
            return false;
        }
        rxQueue_ = xQueueCreate(8, sizeof(rmt_rx_done_event_data_t));
        if (!rxQueue_)
        {
            ESP_LOGE(kTag, "RX begin failed: queue create");
            rmt_del_channel(rxChannel_);
            rxChannel_ = nullptr;
            return false;
        }
        rxPendingMask_ = 0;
        rxNeedRestart_ = false;
        rxBuffers_[0].assign(rxBufferSymbols_, {});
        rxBuffers_[1].assign(rxBufferSymbols_, {});
        rxCallbackCtx_ = {};
        rxCallbackCtx_.queue = rxQueue_;
        rxCallbackCtx_.overflowFlag = &rxOverflowed_;
        rxCallbackCtx_.buffers[0] = rxBuffers_[0].data();
        rxCallbackCtx_.buffers[1] = rxBuffers_[1].data();
        rxCallbackCtx_.bufferLenSymbols = rxBufferSymbols_;
        rxCallbackCtx_.pendingMask = &rxPendingMask_;
        rxCallbackCtx_.rxConfig = &rxConfig_;
        rxCallbackCtx_.channel = rxChannel_;
        rxCallbackCtx_.needRestart = &rxNeedRestart_;
        rmt_rx_event_callbacks_t cbs = {
            .on_recv_done = rxDoneCallback,
        };
        if (rmt_rx_register_event_callbacks(rxChannel_, &cbs, &rxCallbackCtx_) != ESP_OK)
        {
            ESP_LOGE(kTag, "RX begin failed: register callbacks");
            vQueueDelete(rxQueue_);
            rxQueue_ = nullptr;
            rmt_del_channel(rxChannel_);
            rxChannel_ = nullptr;
            return false;
        }
        // Resolve effective RX parameters once at begin (per spec).
        RxParams params = defaultParams(useRawOnly_ || useRawPlusKnown_);
        if (!useRawOnly_)
        {
            const auto &plist = protocols_.empty() ? (useKnownNoAC_ ? knownWithoutAC() : allKnownProtocols()) : protocols_;
            for (auto proto : plist)
            {
                mergeParams(params, recommendedParamsForProtocol(proto));
            }
        }
        if (frameGapUs_ > 0)
            params.frameGapUs = frameGapUs_;
        if (hardGapUs_ > 0)
            params.hardGapUs = hardGapUs_;
        if (minFrameUs_ > 0)
            params.minFrameUs = minFrameUs_;
        if (maxFrameUs_ > 0)
            params.maxFrameUs = maxFrameUs_;
        if (minEdges_ > 0)
            params.minEdges = minEdges_;
        if (frameCountMax_ > 0)
            params.frameCountMax = frameCountMax_;
        if (splitPolicySet_)
            params.splitPolicy = splitPolicy_;

        effFrameGapUs_ = params.frameGapUs;
        effHardGapUs_ = params.hardGapUs;
        effMinFrameUs_ = params.minFrameUs;
        effMaxFrameUs_ = params.maxFrameUs;
        effMinEdges_ = params.minEdges;
        effFrameCountMax_ = params.frameCountMax;
        effSplitPolicy_ = params.splitPolicy;

        // RMT symbol range: set max to the longest expected mark/space among merged params (capped by RMT limit).
        uint32_t maxSymbolUs = std::max(effFrameGapUs_, effHardGapUs_);
        if (maxSymbolUs == 0)
            maxSymbolUs = 20000; // fallback to default hardGap
        // Hardware tick upper limit scales with resolution; base is ~65.5ms at 1us ticks.
        const uint64_t kRmtBaseMaxNs = 65000000ULL; // at 1us resolution
        uint64_t desiredMaxNs = static_cast<uint64_t>(maxSymbolUs) * 1000ULL;
        uint64_t scaledMaxNs = kRmtBaseMaxNs * std::max<uint16_t>(1, quantizeT_);
        if (desiredMaxNs > scaledMaxNs)
            desiredMaxNs = scaledMaxNs;
        rxConfig_.signal_range_min_ns = 1000;
        rxConfig_.signal_range_max_ns = desiredMaxNs;
        if (rmt_receive(rxChannel_, rxBuffers_[0].data(), rxBufferSymbols_ * sizeof(rmt_symbol_word_t), &rxConfig_) != ESP_OK)
        {
            ESP_LOGE(kTag, "RX begin failed: rmt_receive");
            vQueueDelete(rxQueue_);
            rxQueue_ = nullptr;
            rmt_del_channel(rxChannel_);
            rxChannel_ = nullptr;
            return false;
        }

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

        const char *modeStr = useRawOnly_ ? "RAW_ONLY" : (useRawPlusKnown_ ? "RAW_PLUS_KNOWN" : (useKnownNoAC_ ? "KNOWN_NO_AC" : "KNOWN_ONLY"));
        ESP_LOGD(kTag, "RX init version=%s pin=%d invert=%s T_us=%u mode=%s frameGapUs=%u hardGapUs=%u minFrameUs=%u maxFrameUs=%u minEdges=%u frameCountMax=%u splitPolicy=%s protocols=%u",
                 ESP32IRPULSECODEC_VERSION_STR,
                 rxPin_, invertInput_ ? "true" : "false", static_cast<unsigned>(quantizeT_),
                 modeStr,
                 static_cast<unsigned>(effFrameGapUs_),
                 static_cast<unsigned>(effHardGapUs_),
                 static_cast<unsigned>(effMinFrameUs_),
                 static_cast<unsigned>(effMaxFrameUs_),
                 static_cast<unsigned>(effMinEdges_),
                 static_cast<unsigned>(effFrameCountMax_),
                 splitPolicyName(effSplitPolicy_),
                 static_cast<unsigned>(protocols_.size()));
        begun_ = true;
        ESP_LOGI(kTag, "RX begin: pin=%d invert=%s T_us=%u mode=%s",
                 rxPin_, invertInput_ ? "true" : "false", static_cast<unsigned>(quantizeT_),
                 modeStr);
        return true;
    }
    void Receiver::end()
    {
        if (!begun_)
        {
            return;
        }
        if (rxChannel_)
        {
            rmt_disable(rxChannel_);
            rmt_del_channel(rxChannel_);
            rxChannel_ = nullptr;
        }
        if (rxQueue_)
        {
            vQueueDelete(rxQueue_);
            rxQueue_ = nullptr;
        }
        pendingSegments_.clear();
        rxOverflowed_ = false;
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

        void normalizeSeq(std::vector<int8_t> &seq)
        {
            std::vector<int8_t> out;
            out.reserve(seq.size());
            for (int v : seq)
            {
                if (v == 0)
                {
                    continue;
                }
                if (out.empty() || (out.back() > 0) != (v > 0))
                {
                    out.push_back(static_cast<int8_t>(v));
                    continue;
                }
                int last = out.back();
                out.pop_back();
                int total = last + v;
                int sign = (total >= 0) ? 1 : -1;
                int remaining = total * sign;
                while (remaining > 127)
                {
                    out.push_back(static_cast<int8_t>(sign * 127));
                    remaining -= 127;
                }
                if (remaining > 0)
                {
                    out.push_back(static_cast<int8_t>(sign * remaining));
                }
            }
            seq.swap(out);
        }

        bool appendFrameIfValid(const std::vector<int8_t> &seq, uint16_t T_us, const RxParams &params, std::vector<std::vector<int8_t>> &outFrames, bool allowShort = false)
        {
            if (seq.empty())
            {
                return true;
            }
            uint32_t totalUs = 0;
            for (int v : seq)
            {
                int mag = v < 0 ? -v : v;
                totalUs += static_cast<uint32_t>(mag) * static_cast<uint32_t>(T_us);
            }
            if (!allowShort && (seq.size() < params.minEdges || totalUs < params.minFrameUs))
            {
                return true; // treat as noise; not an error
            }
            outFrames.push_back(seq);
            if (params.frameCountMax == 0)
            {
                return true;
            }
            return outFrames.size() <= params.frameCountMax;
        }

        bool splitITPSByGap(const esp32ir::ITPSBuffer &buf, uint32_t gapUs, esp32ir::ITPSBuffer &firstOut, esp32ir::ITPSBuffer &secondOut)
        {
            if (buf.frameCount() == 0 || gapUs == 0)
            {
                return false;
            }
            const auto &f = buf.frame(0);
            if (!f.seq || f.len == 0 || f.T_us == 0)
            {
                return false;
            }
            uint32_t maxSpaceUs = 0;
            size_t maxStart = 0;
            size_t maxLen = 0;
            uint32_t runUs = 0;
            size_t runStart = 0;
            for (uint16_t i = 0; i < f.len; ++i)
            {
                int v = f.seq[i];
                if (v < 0)
                {
                    if (runUs == 0)
                        runStart = i;
                    runUs += static_cast<uint32_t>((-v) * f.T_us);
                }
                else
                {
                    if (runUs > maxSpaceUs)
                    {
                        maxSpaceUs = runUs;
                        maxStart = runStart;
                        maxLen = i - runStart;
                    }
                    runUs = 0;
                }
            }
            if (runUs > maxSpaceUs)
            {
                maxSpaceUs = runUs;
                maxStart = runStart;
                maxLen = f.len - runStart;
            }
            if (maxSpaceUs < gapUs || maxLen == 0)
            {
                return false;
            }
            if (maxStart == 0 || (maxStart + maxLen) >= f.len)
            {
                return false;
            }
            std::vector<int8_t> seq1;
            seq1.reserve(f.len);
            for (size_t i = 0; i < maxStart; ++i)
            {
                seq1.push_back(f.seq[i]);
            }
            std::vector<int8_t> seq2;
            seq2.reserve(f.len - maxStart - maxLen);
            for (size_t i = maxStart + maxLen; i < f.len; ++i)
            {
                seq2.push_back(f.seq[i]);
            }
            if (seq1.empty() || seq2.empty())
            {
                return false;
            }
            firstOut.clear();
            secondOut.clear();
            esp32ir::ITPSFrame f1{f.T_us, static_cast<uint16_t>(seq1.size()), seq1.data(), 0};
            esp32ir::ITPSFrame f2{f.T_us, static_cast<uint16_t>(seq2.size()), seq2.data(), 0};
            firstOut.addFrame(f1);
            secondOut.addFrame(f2);
            return true;
        }
    } // namespace

    bool Receiver::decode(const esp32ir::ITPSBuffer &buf, esp32ir::RxResult &out, bool overflowed)
    {
        auto fillRaw = [&](esp32ir::RxStatus status) -> bool
        {
            out.status = status;
            out.protocol = esp32ir::Protocol::RAW;
            out.message = {esp32ir::Protocol::RAW, nullptr, 0, 0};
            out.raw = buf;
            out.payloadStorage.clear();
            return true;
        };

        if (overflowed)
        {
            return fillRaw(esp32ir::RxStatus::OVERFLOW);
        }
        if (useRawOnly_)
        {
            return fillRaw(esp32ir::RxStatus::RAW_ONLY);
        }

        auto fillDecoded = [&](esp32ir::Protocol proto, const void *payload, size_t len, const esp32ir::ITPSBuffer *rawPtr = nullptr) -> bool
        {
            out.payloadStorage.assign(reinterpret_cast<const uint8_t *>(payload),
                                      reinterpret_cast<const uint8_t *>(payload) + len);
            out.message = {proto, out.payloadStorage.data(), static_cast<uint16_t>(len), 0};
            out.protocol = proto;
            out.status = esp32ir::RxStatus::DECODED;
            if (useRawPlusKnown_)
            {
                out.raw = rawPtr ? *rawPtr : buf;
            }
            else
            {
                out.raw.clear();
            }
            return true;
        };

        auto protocolsToTry = protocols_.empty() ? (useKnownNoAC_ ? knownWithoutAC() : allKnownProtocols()) : protocols_;

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
                    esp32ir::ITPSBuffer firstBuf;
                    esp32ir::ITPSBuffer secondBuf;
                    const esp32ir::ITPSBuffer *rawPtr = &buf;
                    uint32_t protoGap = recommendedParamsForProtocol(proto).frameGapUs;
                    if (protoGap > 0 && splitITPSByGap(buf, protoGap, firstBuf, secondBuf))
                    {
                        rawPtr = &firstBuf;
                        pendingSegments_.push_back({std::move(secondBuf), overflowed});
                    }
                    return fillDecoded(proto, &p, sizeof(p), rawPtr);
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
        if (useRawPlusKnown_)
        {
            return fillRaw(esp32ir::RxStatus::RAW_ONLY);
        }
        return false;
    }

    bool Receiver::poll(esp32ir::RxResult &out)
    {
        if (!begun_)
        {
            ESP_LOGW(kTag, "RX poll called before begin");
        }
        if (!rxQueue_)
        {
            return false;
        }
        if (!rxChannel_)
        {
            return false;
        }
        if (!pendingSegments_.empty())
        {
            auto pending = std::move(pendingSegments_.front());
            pendingSegments_.pop_front();
            return decode(pending.raw, out, pending.overflowed);
        }
        rmt_rx_done_event_data_t ev = {};
        if (xQueueReceive(rxQueue_, &ev, 0) != pdTRUE)
        {
            return false;
        }
        int bufferIndex = -1;
        for (uint8_t i = 0; i < 2; ++i)
        {
            if (rxBuffers_[i].data() == ev.received_symbols)
            {
                bufferIndex = static_cast<int>(i);
                break;
            }
        }
        bool truncated = ev.num_symbols >= rxBufferSymbols_;
        bool overflowed = rxOverflowed_ || (ev.num_symbols == 0) || (ev.received_symbols == nullptr) || (!ev.flags.is_last);
        rxOverflowed_ = false;
        ESP_LOGV(kTag, "RX RMT symbols=%u last=%d invert=%s T_us=%u",
                 static_cast<unsigned>(ev.num_symbols),
                 static_cast<int>(ev.flags.is_last),
                 invertInput_ ? "true" : "false",
                 static_cast<unsigned>(quantizeT_));
#if LOG_LEVEL >= LOG_LEVEL_VERBOSE
        if (ev.received_symbols)
        {
            // Dump first few symbols to help debugging (verbose only).
            char buf[1024];
            size_t pos = 0;
            for (size_t i = 0; i < ev.num_symbols && pos + 30 < sizeof(buf); ++i)
            {
                const auto &sym = ev.received_symbols[i];
                int n = snprintf(buf + pos, sizeof(buf) - pos, "[%u]{%u,%u}{%u,%u} ",
                                 static_cast<unsigned>(i),
                                 static_cast<unsigned>(sym.level0), static_cast<unsigned>(sym.duration0),
                                 static_cast<unsigned>(sym.level1), static_cast<unsigned>(sym.duration1));
                if (n > 0)
                    pos += static_cast<size_t>(n);
            }
            buf[std::min(pos, sizeof(buf) - 1)] = '\0';
            ESP_LOGV(kTag, "RX RMT dump: %s%s", buf, (pos + 30 < sizeof(buf)) ? "..." : "");
        }
#endif
        std::vector<int8_t> seq;
        seq.reserve(ev.num_symbols * 2);
        for (size_t i = 0; i < ev.num_symbols; ++i)
        {
            const auto &sym = ev.received_symbols[i];
            if (sym.duration0)
            {
                // invertInput_ is already applied by RMT hardware (flags.invert_in).
                bool mark = sym.level0 != 0;
                uint32_t durUs = static_cast<uint32_t>(sym.duration0) * static_cast<uint32_t>(quantizeT_);
                pushSeq(seq, mark, durUs, quantizeT_);
            }
            if (sym.duration1)
            {
                bool mark = sym.level1 != 0;
                uint32_t durUs = static_cast<uint32_t>(sym.duration1) * static_cast<uint32_t>(quantizeT_);
                pushSeq(seq, mark, durUs, quantizeT_);
            }
        }
        // restart reception
        // rmt_receive is re-armed in ISR; if pending buffers exhausted and restart flagged, try here.
        if (rxNeedRestart_)
        {
            int freeIdx = -1;
            uint8_t mask = rxPendingMask_;
            for (uint8_t i = 0; i < 2; ++i)
            {
                if ((mask & (1u << i)) == 0)
                {
                    freeIdx = static_cast<int>(i);
                    break;
                }
            }
            if (freeIdx >= 0)
            {
                esp_err_t rxErr = rmt_receive(rxChannel_, rxBuffers_[freeIdx].data(), rxBufferSymbols_ * sizeof(rmt_symbol_word_t), &rxConfig_);
                if (rxErr == ESP_OK)
                {
                    rxNeedRestart_ = false;
                }
                else
                {
                    ESP_LOGW(kTag, "RX rmt_receive restart failed err=%d", static_cast<int>(rxErr));
                    overflowed = true;
                }
            }
        }

        while (!seq.empty() && seq.front() < 0)
        {
            seq.erase(seq.begin());
        }
        normalizeSeq(seq);
        if (seq.empty() || seq.front() < 0)
        {
            return false;
        }
        if (truncated)
        {
            ESP_LOGW(kTag, "RX buffer truncated (symbols=%zu cap=%zu)", static_cast<size_t>(ev.num_symbols), rxBufferSymbols_);
            overflowed = true;
        }

        RxParams params{};
        params.frameGapUs = effFrameGapUs_;
        params.hardGapUs = effHardGapUs_;
        params.minFrameUs = effMinFrameUs_;
        params.maxFrameUs = effMaxFrameUs_;
        params.minEdges = effMinEdges_;
        params.frameCountMax = effFrameCountMax_;
        params.splitPolicy = effSplitPolicy_;
        if (params.frameGapUs == 0)
        {
            RxParams def = defaultParams(useRawOnly_ || useRawPlusKnown_);
            params = def;
        }

        std::vector<std::vector<int8_t>> framesData;
        std::vector<int8_t> current;
        uint32_t currentTimeUs = 0;
        uint32_t spaceRunUs = 0;
        size_t spaceRunStartIndex = 0;
        uint32_t maxSpaceRunUs = 0;
        // Allow a small tolerance when deciding gaps to cope with measurement jitter.
        const uint32_t gapToleranceUs = params.frameGapUs ? std::max<uint32_t>(quantizeT_, params.frameGapUs / 20) : quantizeT_;
        const uint32_t hardGapToleranceUs = params.hardGapUs ? std::max<uint32_t>(quantizeT_, params.hardGapUs / 20) : quantizeT_;
        ESP_LOGD(kTag, "RX split params frameGapUs=%u hardGapUs=%u tol=%u/%u splitPolicy=%s",
                 static_cast<unsigned>(params.frameGapUs),
                 static_cast<unsigned>(params.hardGapUs),
                 static_cast<unsigned>(gapToleranceUs),
                 static_cast<unsigned>(hardGapToleranceUs),
                 splitPolicyName(params.splitPolicy));
        auto totalTimeUs = [&](const std::vector<int8_t> &s) -> uint32_t
        {
            uint32_t total = 0;
            for (int v : s)
            {
                int mag = v < 0 ? -v : v;
                total += static_cast<uint32_t>(mag) * static_cast<uint32_t>(quantizeT_);
            }
            return total;
        };
        auto flush = [&](bool &overflowFlag, bool allowShort)
        {
            if (!current.empty())
            {
                size_t before = framesData.size();
                if (!appendFrameIfValid(current, quantizeT_, params, framesData, allowShort))
                {
                    overflowFlag = true;
                }
                if (framesData.size() > before)
                {
                    ESP_LOGD(kTag, "RX frame appended edges=%u time_us=%u allowShort=%d",
                             static_cast<unsigned>(framesData.back().size()),
                             static_cast<unsigned>(totalTimeUs(framesData.back())),
                             allowShort ? 1 : 0);
                }
                current.clear();
                currentTimeUs = 0;
            }
        };

        for (size_t i = 0; i < seq.size(); ++i)
        {
            int v = seq[i];
            uint32_t durUs = static_cast<uint32_t>((v < 0 ? -v : v) * quantizeT_);
            bool isSpace = v < 0;

            if (isSpace)
            {
                if (spaceRunUs == 0)
                {
                    spaceRunStartIndex = current.size();
                }
                spaceRunUs += durUs;
                if (spaceRunUs > maxSpaceRunUs)
                {
                    maxSpaceRunUs = spaceRunUs;
                }
            }
            else
            {
                spaceRunUs = 0;
            }

            bool forceSplit = false;
            if (isSpace && params.hardGapUs > 0 && (spaceRunUs + hardGapToleranceUs) >= params.hardGapUs)
            {
                forceSplit = true;
            }
            else if (isSpace && params.maxFrameUs > 0 && (currentTimeUs + durUs) > params.maxFrameUs && !current.empty())
            {
                forceSplit = true;
            }

            if (forceSplit && !current.empty())
            {
                ESP_LOGD(kTag, "RX split by %s: spaceRunUs=%u durUs=%u frameGapUs=%u hardGapUs=%u currEdges=%u currTimeUs=%u",
                         (params.hardGapUs > 0 && (spaceRunUs + hardGapToleranceUs) >= params.hardGapUs) ? "hardGap" : "maxFrame",
                         static_cast<unsigned>(spaceRunUs),
                         static_cast<unsigned>(durUs),
                         static_cast<unsigned>(params.frameGapUs),
                         static_cast<unsigned>(params.hardGapUs),
                         static_cast<unsigned>(current.size()),
                         static_cast<unsigned>(currentTimeUs));
                if (params.splitPolicy == esp32ir::RxSplitPolicy::KEEP_GAP_IN_FRAME)
                {
                    current.push_back(static_cast<int8_t>(v));
                    currentTimeUs += durUs;
                }
                else
                {
                    // drop the accumulated gap from the frame
                    if (current.size() > spaceRunStartIndex)
                    {
                        current.resize(spaceRunStartIndex);
                    }
                    if (currentTimeUs > spaceRunUs)
                    {
                        currentTimeUs -= spaceRunUs;
                    }
                    else
                    {
                        currentTimeUs = 0;
                    }
                }
                flush(overflowed, /*allowShort=*/true);
                spaceRunUs = 0;
                continue;
            }

            if (current.empty() && isSpace)
            {
                spaceRunUs = 0;
                continue;
            }
            current.push_back(static_cast<int8_t>(v));
            currentTimeUs += durUs;

            if (isSpace && params.frameGapUs > 0 && (spaceRunUs + gapToleranceUs) >= params.frameGapUs)
            {
                ESP_LOGD(kTag, "RX gap flush: spaceRunUs=%u frameGapUs=%u tol=%u edges=%u timeUs=%u",
                         static_cast<unsigned>(spaceRunUs),
                         static_cast<unsigned>(params.frameGapUs),
                         static_cast<unsigned>(gapToleranceUs),
                         static_cast<unsigned>(current.size()),
                         static_cast<unsigned>(currentTimeUs));
                if (params.splitPolicy == esp32ir::RxSplitPolicy::KEEP_GAP_IN_FRAME)
                {
                    flush(overflowed, /*allowShort=*/true);
                }
                else
                {
                    // drop the accumulated gap from the frame
                    if (current.size() >= spaceRunStartIndex)
                    {
                        current.resize(spaceRunStartIndex);
                    }
                    if (currentTimeUs > spaceRunUs)
                    {
                        currentTimeUs -= spaceRunUs;
                    }
                    else
                    {
                        currentTimeUs = 0;
                    }
                    flush(overflowed, /*allowShort=*/true);
                }
                spaceRunUs = 0;
            }
        }
        bool allowShortFinal = (current.size() < params.minEdges) && (currentTimeUs >= params.minFrameUs);
        ESP_LOGD(kTag, "RX split loop end edges=%u timeUs=%u maxSpaceRunUs=%u frameGapUs=%u hardGapUs=%u allowShortFinal=%d",
                 static_cast<unsigned>(current.size()),
                 static_cast<unsigned>(currentTimeUs),
                 static_cast<unsigned>(maxSpaceRunUs),
                 static_cast<unsigned>(params.frameGapUs),
                 static_cast<unsigned>(params.hardGapUs),
                 allowShortFinal ? 1 : 0);
        flush(overflowed, /*allowShort=*/allowShortFinal);

        if (framesData.empty())
        {
            if (bufferIndex >= 0)
            {
                rxPendingMask_ &= static_cast<uint8_t>(~(1u << bufferIndex));
            }
            return false;
        }
        std::vector<PendingSegment> newSegments;
        newSegments.reserve(framesData.size());
        for (const auto &fseq : framesData)
        {
            esp32ir::ITPSBuffer buf;
            esp32ir::ITPSFrame frame{quantizeT_, static_cast<uint16_t>(fseq.size()), fseq.data(), 0};
            buf.addFrame(frame);
            newSegments.push_back({std::move(buf), overflowed});
        }
        if (newSegments.empty())
        {
            if (bufferIndex >= 0)
            {
                rxPendingMask_ &= static_cast<uint8_t>(~(1u << bufferIndex));
            }
            return false;
        }
        ESP_LOGD(kTag, "RX segmented frames=%u pending_before=%u", static_cast<unsigned>(newSegments.size()), static_cast<unsigned>(pendingSegments_.size()));
        PendingSegment first = std::move(newSegments.front());
        for (size_t i = 1; i < newSegments.size(); ++i)
        {
            pendingSegments_.push_back(std::move(newSegments[i]));
        }
        if (bufferIndex >= 0)
        {
            rxPendingMask_ &= static_cast<uint8_t>(~(1u << bufferIndex));
        }
        return decode(first.raw, out, first.overflowed);
    }

} // namespace esp32ir
