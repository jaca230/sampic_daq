#ifndef FRONTEND_EVENT_BANK_H
#define FRONTEND_EVENT_BANK_H

#include <string>
#include <cstdint>
#include <cstddef>
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
        bank_prefix_ = prefix;
    }

    /// Retrieve the prefix.
    const std::string& bankPrefix() const { return bank_prefix_; }

    /// Pointer to start of serialized bank data (zero-copy reference)
    virtual const uint8_t* data() const = 0;

    /// Total size in bytes of serialized data
    virtual size_t size() const = 0;

protected:
    std::string bank_prefix_{"XX"};
};

#endif // FRONTEND_EVENT_BANK_H
