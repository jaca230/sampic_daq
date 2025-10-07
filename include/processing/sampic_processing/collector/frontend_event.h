#ifndef FRONTEND_EVENT_H
#define FRONTEND_EVENT_H

#include <vector>
#include <memory>
#include <chrono>
#include <string>
#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"

/**
 * @class FrontendEvent
 * @brief Represents an aggregated frontend event assembled from one or more
 *        hardware-level SAMPIC events.
 *
 * Each FrontendEvent may contain multiple data banks providing payloads
 * or metadata. Timing information is delegated to timing banks.
 * The event also tracks whether it has been consumed downstream.
 */
class FrontendEvent {
public:
    /** @brief Default constructor. */
    FrontendEvent() = default;

    /**
     * @brief Construct a new FrontendEvent with a given timestamp.
     * @param ts Event creation or reference timestamp.
     */
    explicit FrontendEvent(std::chrono::steady_clock::time_point ts);

    /** @brief Virtual destructor. */
    virtual ~FrontendEvent();

    // ------------------------------------------------------------------
    // Timestamp accessors
    // ------------------------------------------------------------------

    /**
     * @brief Set the event timestamp.
     * @param ts The timestamp to assign.
     */
    void setTimestamp(std::chrono::steady_clock::time_point ts);

    /**
     * @brief Retrieve the event timestamp.
     * @return The timestamp associated with this event.
     */
    std::chrono::steady_clock::time_point timestamp() const;

    // ------------------------------------------------------------------
    // Bank management
    // ------------------------------------------------------------------

    /**
     * @brief Add a data bank to the event.
     * @param bank Shared pointer to the bank to attach.
     */
    void addBank(const std::shared_ptr<FrontendEventBank>& bank);

    /**
     * @brief Access all banks (const).
     * @return Const reference to the vector of attached banks.
     */
    const std::vector<std::shared_ptr<FrontendEventBank>>& banks() const;

    /**
     * @brief Access all banks (mutable).
     * @return Reference to the vector of attached banks.
     */
    std::vector<std::shared_ptr<FrontendEventBank>>& banks();

    /**
     * @brief Find a bank by its prefix string.
     * @param prefix The prefix to search for.
     * @return Shared pointer to the matching bank, or nullptr if not found.
     */
    std::shared_ptr<FrontendEventBank> findBankByPrefix(const std::string& prefix) const;

    /** @brief Remove all attached banks. */
    void clearBanks();

    /** @brief Return total number of attached banks. */
    size_t numBanks() const;

    /** @brief Return total combined size of all banks' data payloads. */
    size_t totalDataSize() const;

    // ------------------------------------------------------------------
    // Consumption state
    // ------------------------------------------------------------------

    /**
     * @brief Mark the event as consumed or unconsumed.
     * @param value True if the event has been processed downstream.
     */
    void markConsumed(bool value = true);

    /**
     * @brief Check whether the event has been consumed.
     * @return True if the event has been marked as consumed.
     */
    bool consumed() const;

    // ------------------------------------------------------------------
    // Optional finalization hook
    // ------------------------------------------------------------------

    /**
     * @brief Optional finalization hook (no-op by default).
     *
     * Derived classes may override this to compute derived quantities
     * or perform final validation before the event is published.
     */
    virtual void finalize();

private:
    std::chrono::steady_clock::time_point timestamp_{}; ///< Event timestamp
    std::vector<std::shared_ptr<FrontendEventBank>> banks_; ///< Attached data banks
    bool consumed_{false}; ///< Indicates if this event has been processed downstream
};

#endif // FRONTEND_EVENT_H
