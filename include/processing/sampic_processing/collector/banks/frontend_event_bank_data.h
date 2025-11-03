#ifndef FRONTEND_EVENT_BANK_DATA_H
#define FRONTEND_EVENT_BANK_DATA_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"
#include "integration/sampic/collector/sampic_event.h"
#include <vector>
#include <memory>
#include <cstring>
#if defined(__AVX2__)
#include <immintrin.h>
#endif

/// Bank representing waveform and scalar data from multiple SampicEvents.
/// Holds shared ownership of the contributing SampicEvents to guarantee that
/// zero-copy slices remain valid while the bank exists.
class FrontendEventBankData : public FrontendEventBank {
public:
    FrontendEventBankData(std::vector<std::shared_ptr<SampicEvent>> parents,
                          const std::vector<const HitStruct*>& hits);

    const uint8_t* data() const override { return nullptr; }  // multi-slice
    size_t size() const override { return total_size_; }

    void writeTo(uint8_t* dest) const override {
        for (const auto& [ptr, len] : slices_) {
#if defined(__AVX2__)
            size_t offset = 0;
            const uint8_t* src = ptr;
            for (; offset + 32 <= len; offset += 32) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + offset));
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest + offset), chunk);
            }
            for (; offset + 16 <= len; offset += 16) {
                __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + offset));
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dest + offset), chunk);
            }
            if (offset < len) {
                std::memcpy(dest + offset, src + offset, len - offset);
            }
#else
            std::memcpy(dest, ptr, len);
#endif
            dest += len;
        }
    }

    const std::vector<std::pair<const uint8_t*, size_t>>& slices() const { return slices_; }

private:
    std::vector<std::pair<const uint8_t*, size_t>> slices_;
    std::vector<std::shared_ptr<SampicEvent>> parent_refs_;  ///< Hold parents alive for zero-copy slices
    size_t total_size_{0};
};

#endif // FRONTEND_EVENT_BANK_DATA_H
