#ifndef FRONTEND_EVENT_BANK_H
#define FRONTEND_EVENT_BANK_H

#include <string>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>

/// Base class for all frontend event banks.
/// Each bank represents one contiguous memory region (zero-copy view)
/// suitable for direct MIDAS bank serialization.
class FrontendEventBank {
public:
    FrontendEventBank() = default;
    virtual ~FrontendEventBank() = default;

    /// Set the 2-character MIDAS bank prefix (e.g. "AD", "AT").
    /// Throws if prefix is not exactly 2 characters.
    void setBankPrefix(const std::string& prefix) {
        if (prefix.size() != 2)
            throw std::invalid_argument("FrontendEventBank prefix must be exactly 2 characters");
        bank_prefix_[0] = prefix[0];
        bank_prefix_[1] = prefix[1];
        bank_prefix_[2] = '\0';
    }

    /// Retrieve the prefix as a C string.
    const char* bankPrefix() const { return bank_prefix_; }

    /// Pointer to start of serialized bank data (zero-copy reference)
    virtual const uint8_t* data() const = 0;

    /// Total size in bytes of serialized data
    virtual size_t size() const = 0;

    /// Write bank data to destination buffer (optimized for each bank type)
    /// Default implementation does single memcpy; overridden for multi-slice banks
    virtual void writeTo(uint8_t* dest) const {
        const uint8_t* src = data();
        if (src) {
            std::memcpy(dest, src, size());
        }
    }

protected:
    char bank_prefix_[3] = {'X', 'X', '\0'};  ///< 2-char prefix + null terminator
};

#endif // FRONTEND_EVENT_BANK_H
