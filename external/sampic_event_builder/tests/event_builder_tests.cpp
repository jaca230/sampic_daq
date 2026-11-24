#include "sampic_event_builder/stages/building/fixed_time_bin_event_builder.h"
#include "sampic_event_builder/stages/building/periodic_readout_event_builder.h"
#include "sampic_event_builder/stages/building/time_window_event_builder.h"
#include "sampic_event_builder/stages/selection/topology_selection_stage.h"
#include "sampic_event_builder/pipeline/event_builder_pipeline.h"
#include "sampic_event_builder/banks/data_bank.h"
#include "sampic_event_builder/stages/finalization/product_bank_finalization_stage.h"
#include "sampic_event_builder/stages/selection/passthrough_selection_stage.h"
#include "sampic_event_builder/testing/crate_simulator.h"
#include "sampic_event_builder/testing/vector_input_stage.h"

#include "sampic_event_builder/diagnostics/timing_accumulator.h"
#include <limits>
#include <cassert>
#include <stdexcept>
#include "sampic_event_builder/banks/data_bank.h"
#include <chrono>
#include <cstring>
#include <cstdint>
#include <vector>
#include <memory>

using namespace sampic::event_builder;

int main()
{
    {
        timing::Accumulator accumulator;
        timing::Acquisition a;
        a.hit_count = 10; a.duration = {1, 2, 3, 6}; a.retries = 2;
        const auto bytes = std::span(reinterpret_cast<const std::uint8_t*>(&a), sizeof(a));
        assert(banks::AcquisitionTimingBank::decode(bytes).duration.total_us == 6);
        auto fails = [](auto action) {
            try { action(); } catch (const std::exception&) { return true; }
            return false;
        };
        assert(fails([&] { banks::AcquisitionTimingBank::decode(bytes.first(71)); }));
        auto invalid = a; invalid.version = 2;
        assert(fails([&] { banks::AcquisitionTimingBank::decode({reinterpret_cast<const std::uint8_t*>(&invalid), sizeof(invalid)}); }));
        accumulator.add(0, a); accumulator.add(1, a);
        assert(fails([&] { accumulator.add(0, a); }));
        a.acquisition_id = 2;
        assert(fails([&] { accumulator.add(0, a); }));
        a.acquisition_id = 1; a.hit_count = 0; a.duration.read_us = 5;
        accumulator.add(0, a);
        auto summaries = accumulator.take(7);
        assert(summaries.size() == 2 && summaries.at(0).cycle_id == 7);
        const auto& summary = summaries.at(0);
        assert(summary.read_count == 2 && summary.hit_count == 10);
        assert(summary.sum.read_us == 7 && summary.maximum.read_us == 5);
        assert(summary.retries_sum == 4 && summary.retries_max == 2);
        assert(summary.first_acquisition_id == 0 && summary.last_acquisition_id == 1);
        assert(accumulator.take(8).empty());
        assert(fails([&] { accumulator.add(0, a); }));
        a.acquisition_id = 2; accumulator.add(0, a);
        assert(accumulator.take(9).at(0).read_count == 1);
        accumulator.reset(); a.acquisition_id = 0;
        a.hit_count = std::numeric_limits<std::uint64_t>::max(); accumulator.add(0, a);
        a.acquisition_id = 1; a.hit_count = 1;
        assert(fails([&] { accumulator.add(0, a); }));
        assert(accumulator.take(10).at(0).read_count == 1);
    }
    CrateSimulator first({0, 256, 64, 100'000.0, 0.1, 10});
    CrateSimulator second({1, 256, 64, 100'000.0, 0.1, 20});
    const auto first_hits = first.generate(100);
    const auto second_hits = second.generate(100);
    assert(first_hits.size() == 100 && second_hits.size() == 100);
    assert(first_hits.front().crate == 0 && second_hits.front().crate == 1);

    std::vector<Hit> grouped_hits{
        Hit{0, 0, 0, 100, {1000}, false}, Hit{1, 0, 1, 150, {1000}, false},
        Hit{0, 0, 2, 400, {1000}, false}};
    const auto events = TimeWindowEventBuilder(200).build(grouped_hits);
    assert(events.size() == 2);
    assert(events[0].hits.size() == 2 && events[1].hits.size() == 1);

    TimeWindowEventBuilder held_builder({200, 800, 2});
    assert(held_builder.build({grouped_hits[0]}).empty());
    assert(held_builder.build({grouped_hits[1]}).empty());
    const auto flushed = held_builder.flush();
    assert(flushed.size() == 1);
    assert(flushed.front().hits.size() == 2);

    TimeWindowEventBuilder zero_lateness_builder({100, 0, 2});
    assert(zero_lateness_builder.build({
        Hit{0, 0, 0, 110, {1000}, false},
        Hit{0, 0, 1, 250, {1000}, false},
    }).empty());
    const auto zero_lateness_events = zero_lateness_builder.build({
        Hit{1, 0, 2, 150, {1000}, false},
        Hit{1, 0, 3, 260, {1000}, false},
    });
    assert(zero_lateness_events.size() == 1);
    assert(zero_lateness_events.front().hits.size() == 2);

    FixedTimeBinEventBuilder fixed_bins({100, 0, 0, 2, false, 256});
    assert(fixed_bins.build({
        Hit{0, 0, 0, 110, {1000}, false},
        Hit{0, 0, 2, 250, {1000}, false},
    }).empty());
    auto fixed_events = fixed_bins.build({
        Hit{1, 0, 1, 150, {1000}, false},
        Hit{1, 0, 3, 260, {1000}, false},
    });
    assert(fixed_events.size() == 1);
    assert(fixed_events.front().start_ns == 100);
    assert(fixed_events.front().end_ns == 200);
    assert(fixed_events.front().hits.size() == 2);

    using Clock = PeriodicReadoutEventBuilder::Clock;
    auto now = Clock::time_point{};
    PeriodicReadoutEventBuilder periodic(
        {10'000}, [&now] { return now; });
    assert(periodic.build({
        Hit{0, 0, 0, 110, {1000}, false},
        Hit{1, 0, 1, 150, {1000}, false},
    }).empty());
    now += std::chrono::nanoseconds(10'000);
    const auto periodic_events = periodic.build({});
    assert(periodic_events.size() == 1);
    assert(periodic_events.front().hits.size() == 2);

    // Host-clock collection ignores ordering and progress of hardware clocks.
    now=Clock::time_point{};
    PeriodicReadoutEventBuilder host_window({1000},[&now]{return now;});
    assert(host_window.build({Hit{0,0,0,9'000'000'000,{1},false},
                              Hit{1,0,1,0,{2},false},Hit{0,0,2,1,{3},false}}).empty());
    assert(host_window.readout_deadline()==Clock::time_point{}+std::chrono::nanoseconds(1000));
    now+=std::chrono::nanoseconds(999); assert(host_window.build({}).empty());
    assert(host_window.readout_deadline()==Clock::time_point{}+std::chrono::nanoseconds(1000));
    now+=std::chrono::nanoseconds(1);
    auto host_events=host_window.build({});
    assert(host_window.readout_deadline()==Clock::time_point{}+std::chrono::nanoseconds(2000));
    assert(host_events.size()==1 && host_events[0].hits.size()==3);
    assert(host_events[0].start_ns==0 && host_events[0].end_ns==1000);
    assert(host_events[0].hits[0].timestamp_ns==9'000'000'000 && host_events[0].hits[1].timestamp_ns==0);
    now+=std::chrono::nanoseconds(1000);
    auto empty_window=host_window.build({});
    assert(empty_window.size()==1 && empty_window[0].hits.empty());
    assert(banks::DataBank{}.encode(empty_window[0]).size()==28);

    EventBuilderPipeline pipeline(
        std::make_unique<testing::VectorInputStage>(std::vector<Hit>{first_hits.front()}),
        std::make_unique<TimeWindowEventBuilder>(200),
        std::make_unique<PassthroughSelectionStage>(),
        std::make_unique<ProductBankFinalizationStage>());
    const auto finalized = pipeline.run_once();
    assert(finalized.size() == 1);
    assert(finalized.front().product_bank.size() == 28 + 188);
    auto decoded = banks::DataBank{}.decode(finalized.front().product_bank);
    assert(decoded.start_ns == finalized.front().start_ns);
    assert(decoded.end_ns == finalized.front().end_ns);
    assert(decoded.hits.size() == 1);
    assert(decoded.hits[0].crate == first_hits[0].crate);
    assert(decoded.hits[0].timestamp_ns == first_hits[0].timestamp_ns);
    assert(decoded.hits[0].waveform == first_hits[0].waveform);
    assert(pipeline.metrics().selected_events == 1);

    Event example{100, 200, {Hit{3, 2, 63, 150, {-32768, -1, 0, 32767}, false},
                            Hit{1, 0, 7, 180, {}, false}}};
    auto bytes = banks::DataBank{}.encode(example);
    decoded = banks::DataBank{}.decode(bytes);
    assert(decoded.start_ns == 100 && decoded.end_ns == 200);
    assert(decoded.hits.size() == 2 && decoded.hits[0].waveform == example.hits[0].waveform);
    assert(decoded.hits[0].board == 2 && decoded.hits[0].channel == 63);
    assert(decoded.hits[1].waveform.empty());
    auto rejects = [](const std::vector<std::uint8_t>& data) {
        try { (void)banks::DataBank{}.decode(data); }
        catch (const std::runtime_error&) { return true; }
        return false;
    };
    for (std::size_t length = 0; length < bytes.size(); ++length)
        assert(rejects({bytes.begin(), bytes.begin() + length}));
    auto invalid = bytes; invalid[4] = 2; assert(rejects(invalid));
    invalid = bytes; invalid.push_back(0); assert(rejects(invalid));
    invalid = bytes; std::memset(invalid.data()+8, 255, 4); assert(rejects(invalid));
    invalid = bytes; invalid[28+2+8] = 255; assert(rejects(invalid));
    const auto empty = banks::DataBank{}.encode(Event{300, 400, {}});
    assert(empty.size() == 28);
    assert(banks::DataBank{}.decode(empty).hits.empty());

    EventBuilderPipeline cut_pipeline(
        std::make_unique<testing::VectorInputStage>(grouped_hits),
        std::make_unique<TimeWindowEventBuilder>(200),
        std::make_unique<TopologyEventSelector>(TopologySelectorConfig{2, 1, 1'000'000}),
        std::make_unique<ProductBankFinalizationStage>());
    const auto cut_output=cut_pipeline.run_once();
    assert(cut_output.size() == 2);
    assert(banks::DataBank{}.decode(cut_output[0].product_bank).hits.empty());
    assert(cut_pipeline.metrics().rejected_events == 1);

    TopologyEventSelector selector({2, 1, 1'000'000});
    auto mutable_events=events;
    assert(selector.select(mutable_events[0]) == EventDecision::reject_through_going);
    assert(selector.select(mutable_events[1]) == EventDecision::keep);
}
