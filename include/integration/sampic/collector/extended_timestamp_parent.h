#ifndef SAMPIC_EXTENDED_TIMESTAMP_PARENT_H
#define SAMPIC_EXTENDED_TIMESTAMP_PARENT_H
#include "integration/sampic/collector/sampic_event.h"
#include "integration/sampic/collector/timestamp_unwrapper.h"
#include <vector>

struct ExtendedTimestampParent {
    std::shared_ptr<SampicEvent> owner;
    const HitStruct* hits;
    int hit_count;
};

inline ExtendedTimestampParent extendParentTimestamps(
    const std::shared_ptr<SampicEvent>& parent, SampicTimestampUnwrapper& unwrapper) {
    const auto& event = *parent->data();
    if (event.NbOfHitsInEvent < 0 || event.NbOfHitsInEvent > MAX_EXPECTED_FRAMES)
        throw std::runtime_error("invalid vendor hit count");
    std::vector<double> extended_times;
    extended_times.reserve(event.NbOfHitsInEvent);
    bool needs_extension = false;
    for (int i = 0; i < event.NbOfHitsInEvent; ++i) {
        const double raw = event.Hit[i].FirstCellTimeStamp;
        const double extended = unwrapper.unwrap(raw, parent->timestampModulusNs(), parent->timestamp());
        extended_times.push_back(extended);
        needs_extension |= extended != raw;
    }
    if (!needs_extension) return {parent, event.Hit, event.NbOfHitsInEvent};
    // Copy populated hits only, never the multi-megabyte EventStruct. Aliasing
    // ownership keeps these copies alive for verbose SD's zero-copy slices.
    struct Storage {
        std::shared_ptr<SampicEvent> original;
        std::vector<HitStruct> hits;
    };
    auto storage = std::make_shared<Storage>();
    storage->original = parent;
    storage->hits.assign(event.Hit, event.Hit + event.NbOfHitsInEvent);
    for (int i = 0; i < event.NbOfHitsInEvent; ++i) {
        auto& hit = storage->hits[i];
        const double offset = extended_times[i] - hit.FirstCellTimeStamp;
        hit.FirstCellTimeStamp = extended_times[i];
        hit.AdvancedParams.PhysicalCell0TimeStamp += offset;
    }
    return {std::shared_ptr<SampicEvent>(storage, parent.get()), storage->hits.data(), event.NbOfHitsInEvent};
}
#endif
