#include "sampic_event_builder/banks/selection_summary_bank.h"
#include "mfe.h"
#include "sampic_event_builder/stages/selection/cut_chain.h"
#include "sampic_event_builder/stages/selection/dummy_selection_stage.h"
#include "sampic_event_builder/serialization/midas_bank_writer.h"
#include "sampic_event_builder/diagnostics/timing_accumulator.h"
#include <csignal>
#include <map>

#include "sampic_event_builder/pipeline/event_builder_pipeline.h"
#include "sampic_event_builder/stages/building/fixed_time_bin_event_builder.h"
#include "sampic_event_builder/stages/building/periodic_readout_event_builder.h"
#include "sampic_event_builder/stages/building/time_window_event_builder.h"
#include "sampic_event_builder/stages/finalization/product_bank_finalization_stage.h"
#include "sampic_event_builder/stages/input/midas_raw_input_stage.h"
#include "sampic_event_builder/stages/selection/passthrough_selection_stage.h"
#include "sampic_event_builder/stages/selection/topology_selection_stage.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <set>
#include <sstream>
#include <vector>

using namespace sampic::event_builder;

const char* frontend_name = "SAMPIC Event Builder";
const char* frontend_file_name = __FILE__;
BOOL frontend_call_loop = FALSE;
// The curses-style MFE display busy-polls stdin when output is redirected
// (as in benchmarks/services), which can starve transition RPC handling.
INT display_period = 0;
INT max_event_size = 16 * 1024 * 1024;
INT max_event_size_frag = 0;
INT event_buffer_size = 64 * 1024 * 1024;
BOOL equipment_common_overwrite = FALSE;

// Implemented by MIDAS's ring-buffer support. Keep these narrow declarations
// instead of including msystem.h, whose C compatibility macros conflict with
// the C++ standard library used by the pipeline.
int rb_get_wp(int handle, void** pointer, int timeout_ms);
int rb_increment_wp(int handle, int size);
int rb_get_rp(int handle, void** pointer, int timeout_ms);
int rb_increment_rp(int handle, int size);

namespace {

constexpr short kInputEventId = 1;
struct InputBuffer {
    std::string name;
    HNDLE handle = 0;
    HNDLE request = 0;
};
std::vector<InputBuffer> input_buffers;
MidasRawInputStage* raw_input = nullptr;
std::unique_ptr<EventBuilderPipeline> pipeline;
bool timing_enabled = true;
timing::Accumulator acquisition_timings;
std::uint64_t timing_cycle_id = 0, summarized_reads = 0;
std::atomic<std::uint64_t> diagnostic_events_written{0}, diagnostic_bytes_written{0};
std::thread pipeline_worker;
std::atomic<bool> worker_stop{false};
std::atomic<bool> run_active{false};
std::atomic<std::uint64_t> products_written{0};
std::atomic<std::uint64_t> product_bytes_written{0};
std::mutex pipeline_mutex;
std::vector<FinalizedEvent> pending_products;
std::chrono::steady_clock::time_point run_started_at;
// Protected by pipeline_mutex; retained for the final partial window at EOR.
std::chrono::steady_clock::time_point accounting_start, accounting_run;
PipelineMetrics accounting_before{};
std::uint64_t accounting_events = 0, accounting_hits = 0, accounting_input_ns = 0;
bool accounting_active = false;

template <typename T>
T odb_value(const char* path, T default_value, DWORD type) {
    T value = default_value;
    INT size = sizeof(value);
    const INT status = db_get_value(
        hDB, 0, path, &value, &size, type, TRUE);
    if (status != DB_SUCCESS) {
        cm_msg(MERROR, frontend_name, "Cannot read ODB setting %s (%d)", path, status);
        return default_value;
    }
    return value;
}

std::string odb_string(const char* path, std::string default_value) {
    std::vector<char> value(std::max<std::size_t>(256, default_value.size() + 1), '\0');
    std::memcpy(value.data(), default_value.data(), default_value.size());
    INT size = static_cast<INT>(value.size());
    const INT status = db_get_value(
        hDB, 0, path, value.data(), &size, TID_STRING, TRUE);
    if (status != DB_SUCCESS) {
        cm_msg(MERROR, frontend_name, "Cannot read ODB setting %s (%d)", path, status);
        return default_value;
    }
    return value.data();
}

std::unique_ptr<EventBuildingStage> collection_strategy(
    std::string mode,
    std::size_t expected_crates) {
    std::ranges::transform(mode, mode.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    std::ranges::replace(mode, '-', '_');

    const std::string root="/Equipment/SAMPIC Builder/Settings/Collection_strategies/";
    // Register every strategy's settings, including inactive strategies.
    const auto grouping_window=odb_value<INT>((root+"time_grouping/Event window ns").c_str(),200,TID_INT);
    const auto grouping_lateness=odb_value<INT>((root+"time_grouping/Allowed lateness ns").c_str(),2000,TID_INT);
    const auto bin_width=odb_value<std::uint64_t>((root+"fixed_time_bins/Bin width ns").c_str(),10'000,TID_QWORD);
    const auto bin_origin=odb_value<std::uint64_t>((root+"fixed_time_bins/Bin origin ns").c_str(),0,TID_QWORD);
    const auto bin_lateness=odb_value<std::uint64_t>((root+"fixed_time_bins/Allowed lateness ns").c_str(),1'000'000,TID_QWORD);
    const auto bin_empty=odb_value<BOOL>((root+"fixed_time_bins/Emit empty bins").c_str(),TRUE,TID_BOOL);
    const auto bin_max=odb_value<INT>((root+"fixed_time_bins/Maximum bins per build").c_str(),256,TID_INT);
    const auto period=odb_value<std::uint64_t>((root+"periodic_readout/Period ns").c_str(),10'000'000,TID_QWORD);
    if (mode=="default" || mode=="time_grouping")
        return std::make_unique<TimeWindowEventBuilder>(TimeWindowEventBuilderConfig{
            static_cast<std::uint64_t>(std::max(1,grouping_window)),
            static_cast<std::uint64_t>(std::max(0,grouping_lateness)),expected_crates});
    if (mode=="fixed_time_bins")
        return std::make_unique<FixedTimeBinEventBuilder>(FixedTimeBinEventBuilderConfig{
            std::max<std::uint64_t>(1,bin_width),bin_origin,bin_lateness,expected_crates,bin_empty!=FALSE,
            static_cast<std::size_t>(std::max(1,bin_max))});
    if (mode=="periodic_readout")
        return std::make_unique<PeriodicReadoutEventBuilder>(PeriodicReadoutEventBuilderConfig{
            std::max<std::uint64_t>(1,period)});
    throw std::invalid_argument("unknown collection strategy: " + mode);
}

bool ingest_input_event(void* event_payload) {
    if (!raw_input || !event_payload) return false;
    try {
        std::map<int, std::pair<std::string, std::span<const std::uint8_t>>> raw;
        std::map<int, timing::Acquisition> clocks;
        BANK32* bank = nullptr;
        void* data = nullptr;
        while (const INT size = bk_iterate32(event_payload, &bank, &data)) {
            const std::string name(bank->name, sizeof(bank->name));
            const bool samples = name.starts_with("SR");
            const bool acquisition = timing_enabled && name.starts_with("SA");
            if (!samples && !acquisition) continue;
            if (name[2] < '0' || name[2] > '9' || name[3] < '0' || name[3] > '9')
                throw std::runtime_error("invalid source crate suffix");
            const int crate = (name[2]-'0')*10 + name[3]-'0';
            const std::span bytes{static_cast<const std::uint8_t*>(data), static_cast<std::size_t>(size)};
            if (samples) {
                if (!raw.emplace(crate, std::make_pair(name, bytes)).second)
                    throw std::runtime_error("duplicate SR bank in source event");
            } else if (!clocks.emplace(crate, banks::AcquisitionTimingBank::decode(bytes)).second) {
                throw std::runtime_error("duplicate SA bank in source event");
            }
        }
        if (timing_enabled && raw.size() != clocks.size())
            throw std::runtime_error("each SR bank requires exactly one corresponding SA timing bank");
        for (const auto& [crate, item] : raw) {
            const auto before = raw_input->hits_received();
            if (timing_enabled && !clocks.contains(crate))
                throw std::runtime_error("missing SA bank for " + item.first);
            raw_input->ingest(item.first, item.second);
            if (timing_enabled) {
                const auto& clock = clocks.at(crate);
                if (clock.hit_count != raw_input->hits_received()-before)
                    throw std::runtime_error("SA hit count does not match SR bank");
                acquisition_timings.add(crate, clock);
            }
        }
        return true;
    } catch (const std::exception& error) {
        cm_msg(MERROR, frontend_name, "Fatal source-bank error: %s", error.what());
        return false;
    }
}

} // namespace

EQUIPMENT equipment[] = {
    {"SAMPIC Builder",
     {2, 0, "SYSTEM", EQ_USER, 0, "MIDAS", TRUE, RO_RUNNING,
      10, 0, 0, 0, "", "", ""},
     nullptr},
    {""}
};

namespace {

std::uint64_t elapsed_ns(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now()-start).count();
}

timing::CollectorCycle cycle_record(std::chrono::steady_clock::time_point start,
    const PipelineMetrics& before, std::uint64_t input_events,
    std::uint64_t input_hits, std::uint64_t input_ns, std::size_t output_events) {
    timing::CollectorCycle record;
    record.cycle_id = ++timing_cycle_id;
    record.start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(start-run_started_at).count();
    record.input_events = input_events; record.input_hits = input_hits;
    record.output_events = output_events;
    const auto& after = pipeline->metrics();
    record.output_hits = after.finalization.hits-before.finalization.hits;
    record.selected_events = after.selected_events-before.selected_events;
    record.rejected_events = after.rejected_events-before.rejected_events;
    record.input_read_ns = input_ns;
    record.group_build_ns = after.event_building.processing_nanoseconds-before.event_building.processing_nanoseconds;
    record.cuts_ns = after.selection.processing_nanoseconds-before.selection.processing_nanoseconds;
    record.finalize_ns = after.finalization.processing_nanoseconds-before.finalization.processing_nanoseconds;
    return record;
}

FinalizedEvent diagnostic_event(const timing::CollectorCycle& cycle) {
    FinalizedEvent result;
    if (timing_enabled) {
        for (const auto& [crate, summary] : acquisition_timings.take(cycle.cycle_id)) {
            result.diagnostic_banks.push_back({banks::AcquisitionSummaryBank::name(crate),
                banks::AcquisitionSummaryBank::encode(summary)});
            summarized_reads+=summary.read_count;
        }
        result.diagnostic_banks.push_back({banks::CollectionTimingBank::name,banks::CollectionTimingBank::encode(cycle)});
    }
    result.diagnostic_banks.push_back({banks::SelectionSummaryBank::name,banks::SelectionSummaryBank::encode(cycle.cycle_id,pipeline->take_cut_summaries())});
    return result;
}

void count_output(const FinalizedEvent& built) {
    if (!built.product_bank.empty()) {
        ++products_written;
        product_bytes_written += built.product_bank.size();
    } else {
        ++diagnostic_events_written;
        for (const auto& bank : built.diagnostic_banks) diagnostic_bytes_written += bank.payload.size();
    }
}

bool write_to_ring(const FinalizedEvent& built, int ring_handle) {
    EVENT_HEADER* event = nullptr;
    const INT status = rb_get_wp(
        ring_handle, reinterpret_cast<void**>(&event), 100);
    if (status == DB_TIMEOUT) {
        return false;
    }
    if (status != DB_SUCCESS || event == nullptr) {
        cm_msg(MERROR, frontend_name,
               "Cannot acquire event-builder ring write pointer (%d)", status);
        return false;
    }

    bm_compose_event_threadsafe(
        event, equipment[0].info.event_id, equipment[0].info.trigger_mask,
        0, &equipment[0].serial_number);
    auto* payload = reinterpret_cast<char*>(event + 1);
    event->data_size = write_midas_banks(payload, built, static_cast<std::size_t>(max_event_size));

    const auto total_size = static_cast<int>(sizeof(EVENT_HEADER) + event->data_size);
    const INT commit = rb_increment_wp(ring_handle, total_size);
    if (commit != DB_SUCCESS) {
        cm_msg(MERROR, frontend_name,
               "Cannot commit event-builder ring event (%d)", commit);
        return false;
    }
    count_output(built);
    return true;
}

// MFE drains this ring on its main thread, but not while end_of_run() runs.
// Drain it here before direct transition sends, preserving serial/cycle order.
bool drain_ring_before_transition() {
    const int handle=get_event_rbh(0);
    if (!handle) return true;
    while (true) {
        EVENT_HEADER* event=nullptr;
        const auto status=rb_get_rp(handle,reinterpret_cast<void**>(&event),0);
        if (status==DB_TIMEOUT) return true;
        if (status!=DB_SUCCESS || !event) return false;
        const auto size=static_cast<INT>(sizeof(EVENT_HEADER)+event->data_size);
        if (rpc_send_event(equipment[0].buffer_handle,event,size,BM_WAIT,rpc_mode)!=BM_SUCCESS)
            return false;
        equipment[0].bytes_sent+=size;
        ++equipment[0].events_sent;
        ++equipment[0].events_collected;
        if (rb_increment_rp(handle,size)!=DB_SUCCESS) return false;
    }
}

bool send_from_transition(const FinalizedEvent& built) try {
    std::vector<std::uint8_t> storage(
        static_cast<std::size_t>(max_event_size));
    auto* event = reinterpret_cast<EVENT_HEADER*>(storage.data());
    bm_compose_event_threadsafe(
        event, equipment[0].info.event_id, equipment[0].info.trigger_mask,
        0, &equipment[0].serial_number);
    auto* payload = reinterpret_cast<char*>(event + 1);
    event->data_size = write_midas_banks(payload, built, static_cast<std::size_t>(max_event_size));

    const INT status = rpc_send_event(
        equipment[0].buffer_handle, event,
        static_cast<INT>(sizeof(EVENT_HEADER) + event->data_size),
        BM_WAIT, rpc_mode);
    if (status != BM_SUCCESS) {
        cm_msg(MERROR, frontend_name,
               "Cannot send flushed event-builder product (%d)", status);
        return false;
    }
    equipment[0].bytes_sent+=sizeof(EVENT_HEADER)+event->data_size;
    ++equipment[0].events_sent;
    ++equipment[0].events_collected;
    count_output(built);
    return true;
} catch (const std::overflow_error& error) {
    cm_msg(MERROR, frontend_name, "Cannot flush builder event: %s", error.what());
    return false;
}

void publish_run_counters() {
    std::vector<std::pair<std::string, std::uint64_t>> counters = {
        {"/Equipment/SAMPIC Builder/Variables/Input banks",
         raw_input ? raw_input->banks_received() : 0},
        {"/Equipment/SAMPIC Builder/Variables/Input hits",
         raw_input ? raw_input->hits_received() : 0},
        {"/Equipment/SAMPIC Builder/Variables/Input bank bytes",
         raw_input ? raw_input->bytes_received() : 0},
        {"/Equipment/SAMPIC Builder/Variables/Malformed banks",
         raw_input ? raw_input->malformed_banks() : 0},
        {"/Equipment/SAMPIC Builder/Variables/Output products",
         products_written.load()},
        {"/Equipment/SAMPIC Builder/Variables/Output bytes",
         product_bytes_written.load()},
    };
    const auto elapsed = run_started_at.time_since_epoch().count() == 0
        ? std::chrono::nanoseconds{0}
        : std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::steady_clock::now() - run_started_at);
    counters.emplace_back(
        "/Equipment/SAMPIC Builder/Variables/Metrics elapsed ns",
        static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsed.count())));
    counters.emplace_back("/Equipment/SAMPIC Builder/Variables/Diagnostic events", diagnostic_events_written.load());
    counters.emplace_back("/Equipment/SAMPIC Builder/Variables/Diagnostic bytes", diagnostic_bytes_written.load());
    counters.emplace_back("/Equipment/SAMPIC Builder/Variables/Acquisition timing records", acquisition_timings.reads());
    counters.emplace_back("/Equipment/SAMPIC Builder/Variables/Summarized timing reads", summarized_reads);
    if (pipeline) {
        const auto& metrics = pipeline->metrics();
        const std::pair<const char*, const PipelineStageTotals*> stages[] = {
            {"Event building", &metrics.event_building},
            {"Selection", &metrics.selection},
            {"Finalization", &metrics.finalization},
        };
        for (const auto& [name, stage] : stages) {
            const std::string root = std::string(
                "/Equipment/SAMPIC Builder/Variables/Stages/") + name;
            counters.emplace_back(root + "/Hits", stage->hits);
            counters.emplace_back(root + "/Input bytes", stage->input_bytes);
            counters.emplace_back(root + "/Output bytes", stage->output_bytes);
            counters.emplace_back(root + "/Processing ns", stage->processing_nanoseconds);
        }
        for (const auto& cut:pipeline->cut_totals()) {
            const auto root=std::string("/Equipment/SAMPIC Builder/Variables/Selection/")+cut.id;
            counters.emplace_back(root+"/Evaluated events",cut.evaluated_events);
            counters.emplace_back(root+"/Rejected events",cut.rejected_events);
            counters.emplace_back(root+"/Evaluated hits",cut.evaluated_hits);
            counters.emplace_back(root+"/Rejected hits",cut.rejected_hits);
        }
        counters.emplace_back(
            "/Equipment/SAMPIC Builder/Variables/Selection kept events",
            metrics.selected_events);
        counters.emplace_back(
            "/Equipment/SAMPIC Builder/Variables/Selection rejected events",
            metrics.rejected_events);
    }
    for (const auto& [path, value] : counters) {
        db_set_value(hDB, 0, path.c_str(), &value, sizeof(value), 1, TID_QWORD);
    }
}

void pipeline_worker_loop() {
    constexpr int thread_index = 0;
    constexpr std::size_t max_input_batch = 256;
    signal_readout_thread_active(thread_index, TRUE);
    const int ring_handle = get_event_rbh(thread_index);
    std::vector<std::uint8_t> input_event(static_cast<std::size_t>(max_event_size));
    auto last_metrics_publish = std::chrono::steady_clock::now();
    bool idle = false;
    std::size_t first_source = 0;
    const auto send_or_preserve = [&](FinalizedEvent event) {
        while (!worker_stop.load() && run_active.load() && readout_enabled()) {
            if (write_to_ring(event, ring_handle)) return;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        pending_products.push_back(std::move(event));
    };
    try {
        while (is_readout_thread_enabled() && !worker_stop.load()) {
            if (!run_active.load() || !readout_enabled() || !pipeline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            std::lock_guard lock(pipeline_mutex);
            if (!run_active.load()) continue;
            const auto start = std::chrono::steady_clock::now();
            if (accounting_run != run_started_at) {
                accounting_run = run_started_at;
                accounting_active = false;
                accounting_events = accounting_hits = accounting_input_ns = 0;
            }
            const auto before = pipeline->metrics();
            if (!accounting_active) {
                accounting_start = start;
                accounting_before = before;
                accounting_active = true;
            }
            const auto deadline_before = pipeline->readout_deadline();
            const auto hits_before = raw_input->hits_received();
            std::uint64_t input_events = 0;
            for (std::size_t index = 0; index < input_buffers.size(); ++index) {
                const auto& source = input_buffers[(first_source + index) % input_buffers.size()];
                for (std::size_t count = 0; count < std::max<std::size_t>(1, max_input_batch/input_buffers.size()); ++count) {
                    INT input_size = static_cast<INT>(input_event.size());
                    // Wait only after an idle iteration, on one rotating source.
                    // The millisecond MIDAS timeout bounds other-crate latency.
                    int timeout = BM_NO_WAIT;
                    if (idle && index == 0 && count == 0 && deadline_before &&
                        *deadline_before - std::chrono::steady_clock::now() >= std::chrono::milliseconds(1))
                        timeout = 1;
                    const INT status = bm_receive_event(source.handle, input_event.data(), &input_size, timeout);
                    if (status == BM_ASYNC_RETURN) break;
                    if (status != BM_SUCCESS || !ingest_input_event(reinterpret_cast<EVENT_HEADER*>(input_event.data())+1)) {
                        cm_msg(MERROR, frontend_name, "Stopping builder after invalid source event (status=%d)", status);
                        worker_stop = true; run_active = false;
                        signal_readout_thread_active(thread_index, FALSE);
                        std::raise(SIGTERM);
                        return;
                    }
                    ++input_events;
                }
            }
            first_source = (first_source + 1) % input_buffers.size();
            const auto input_ns = elapsed_ns(start);
            auto events = pipeline->run_once();
            const auto deadline_after = pipeline->readout_deadline();
            const bool periodic = deadline_after.has_value();
            const bool window_closed = deadline_before && deadline_after &&
                *deadline_after != *deadline_before;
            accounting_events += input_events;
            accounting_hits += raw_input->hits_received() - hits_before;
            accounting_input_ns += input_ns;
            idle = input_events == 0;
            if (periodic && !window_closed) {
                if (idle && deadline_after) {
                    // Below MIDAS timeout resolution, sleep directly to the boundary.
                    const auto now = std::chrono::steady_clock::now();
                    if (*deadline_after > now && *deadline_after - now < std::chrono::milliseconds(1))
                        std::this_thread::sleep_until(*deadline_after);
                }
                continue;
            }
            if (!periodic && !input_events && events.empty() &&
                pipeline->metrics().rejected_events == before.rejected_events) {
                accounting_active = false;
                accounting_events = accounting_hits = accounting_input_ns = 0;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            auto cycle = cycle_record(accounting_start, accounting_before,
                accounting_events, accounting_hits, accounting_input_ns, events.size());
            const auto output_start = std::chrono::steady_clock::now();
            for (auto& event : events) send_or_preserve(std::move(event));
            cycle.output_wait_ns = elapsed_ns(output_start);
            cycle.total_ns = elapsed_ns(accounting_start);
            send_or_preserve(diagnostic_event(cycle));
            accounting_active = false;
            accounting_events = accounting_hits = accounting_input_ns = 0;
            const auto metrics_now = std::chrono::steady_clock::now();
            if (metrics_now-last_metrics_publish >= std::chrono::milliseconds(250)) {
                publish_run_counters(); last_metrics_publish = metrics_now;
            }
        }
    } catch (const std::overflow_error& error) {
        worker_stop = true; run_active = false;
        cm_msg(MERROR, frontend_name, "Stopping builder: %s", error.what());
        signal_readout_thread_active(thread_index, FALSE);
        std::raise(SIGTERM);
        return;
    }
    signal_readout_thread_active(thread_index, FALSE);
}

} // namespace

INT frontend_init() {
    // The builder must subscribe before crate producers start and stop only
    // after those producers have flushed their final source banks.
    cm_set_transition_sequence(TR_START, 400);
    cm_set_transition_sequence(TR_STOP, 600);

    timing_enabled = odb_value<BOOL>("/Equipment/SAMPIC Builder/Settings/Timing/Enabled", TRUE, TID_BOOL) != FALSE;
    const auto collection_mode = odb_string(
        "/Equipment/SAMPIC Builder/Settings/Collection_strategies/Active strategy",
        "periodic_readout");
    const auto expected_crates = odb_value<INT>(
        "/Equipment/SAMPIC Builder/Settings/Expected crates", 1, TID_INT);
    auto input=std::make_unique<MidasRawInputStage>(); raw_input=input.get();
    auto selection=std::make_unique<CutChain>();
    const auto cut_root=[](std::string_view id) {
        return std::string("/Equipment/SAMPIC Builder/Settings/Selection/")+std::string(id);
    };
    // Each cut registers under its own required ID, including when disabled.
    const auto register_cut=[&](std::unique_ptr<SelectionStage> cut) {
        const auto root=cut_root(cut->id());
        cut->set_enabled(odb_value<BOOL>((root+"/Enabled").c_str(),FALSE,TID_BOOL)!=FALSE);
        cut->set_priority(odb_value<INT>((root+"/Priority").c_str(),cut->default_priority(),TID_INT));
        selection->add(std::move(cut));
    };
    const auto topology=cut_root(TopologyEventSelector::kId);
    const auto planes=odb_value<INT>((topology+"/Plane count").c_str(),8,TID_INT);
    const auto channels=odb_value<INT>((topology+"/Channels per plane").c_str(),32,TID_INT);
    const auto integral=odb_value<INT>((topology+"/Maximum punch integral per hit").c_str(),3000,TID_INT);
    register_cut(std::make_unique<TopologyEventSelector>(TopologySelectorConfig{
        static_cast<std::size_t>(std::max(2,planes)),static_cast<std::size_t>(std::max(1,channels)),integral}));
    const auto dummy=cut_root(DummyEventSelector::kId);
    const auto every=odb_value<INT>((dummy+"/Reject every N events").c_str(),2,TID_INT);
    if (every<1) { cm_msg(MERROR,frontend_name,"Dummy cut interval must be positive"); return FE_ERR_ODB; }
    register_cut(std::make_unique<DummyEventSelector>(static_cast<std::uint64_t>(every)));

    std::unique_ptr<EventBuildingStage> builder;
    try {
        builder = collection_strategy(
            collection_mode,
            static_cast<std::size_t>(std::max(1, expected_crates)));
    } catch (const std::exception& error) {
        cm_msg(MERROR, frontend_name, "Invalid collection strategy: %s", error.what());
        return FE_ERR_ODB;
    }
    cm_msg(MINFO, frontend_name,
           "Collection strategy '%s' configured for %d SAMPIC stream(s)",
           collection_mode.c_str(), std::max(1, expected_crates));

    pipeline = std::make_unique<EventBuilderPipeline>(
        std::move(input), std::move(builder), std::move(selection),
        std::make_unique<ProductBankFinalizationStage>());

    std::string default_buffers;
    for (int crate = 0; crate < std::max(1, expected_crates); ++crate) {
        char name[16]; std::snprintf(name, sizeof(name), "SBUF%02d", crate);
        if (!default_buffers.empty()) default_buffers += ',';
        default_buffers += name;
    }
    const auto configured = odb_string(
        "/Equipment/SAMPIC Builder/Settings/Input buffers", default_buffers);
    std::istringstream names(configured);
    std::string name;
    std::set<std::string> unique;
    while (std::getline(names, name, ',')) {
        const auto first = name.find_first_not_of(" ");
        const auto last = name.find_last_not_of(" ");
        if (first == std::string::npos) return FE_ERR_ODB;
        name = name.substr(first, last-first+1);
        auto normalized = name;
        std::ranges::transform(normalized, normalized.begin(), [](unsigned char c) { return std::toupper(c); });
        auto output_name = std::string(equipment[0].info.buffer);
        std::ranges::transform(output_name, output_name.begin(), [](unsigned char c) { return std::toupper(c); });
        if (name.size() >= NAME_LENGTH || normalized == output_name || !unique.insert(normalized).second) {
            cm_msg(MERROR, frontend_name, "Input buffer must be unique and separate from output: %s", name.c_str());
            return FE_ERR_ODB;
        }
        input_buffers.push_back({name});
        auto& source = input_buffers.back();
        INT status = bm_open_buffer(name.c_str(), 2 * max_event_size, &source.handle);
        if (status != BM_SUCCESS && status != BM_CREATED) return FE_ERR_HW;
        status = bm_request_event(source.handle, kInputEventId, TRIGGER_ALL,
                                  GET_ALL, &source.request, nullptr);
        if (status != BM_SUCCESS) return FE_ERR_HW;
        cm_msg(MINFO, frontend_name, "Subscribed GET_ALL to %s", name.c_str());
    }
    if (input_buffers.size() != static_cast<std::size_t>(std::max(1, expected_crates))) {
        cm_msg(MERROR, frontend_name, "Input buffer count must match Expected crates");
        return FE_ERR_ODB;
    }
    create_event_rb(0);
    worker_stop = false;
    pipeline_worker = std::thread(pipeline_worker_loop);
    return SUCCESS;
}

INT frontend_exit() {
    worker_stop = true;
    run_active = false;
    stop_readout_threads();
    if (pipeline_worker.joinable()) {
        pipeline_worker.join();
    }
    for (auto& source : input_buffers) {
        if (source.request) bm_delete_request(source.request);
        if (source.handle) bm_close_buffer(source.handle);
    }
    input_buffers.clear();
    pipeline.reset();
    raw_input = nullptr;
    return SUCCESS;
}

INT begin_of_run(INT, char*) {
    cm_msg(MINFO, frontend_name, "Beginning event-builder run");
    if (raw_input != nullptr) {
        raw_input->read_available();
        raw_input->reset_counters();
    }
    products_written = 0;
    product_bytes_written = 0;
    pending_products.clear();
    acquisition_timings.reset(); timing_cycle_id = 0; summarized_reads = 0;
    diagnostic_events_written = 0; diagnostic_bytes_written = 0;
    if (pipeline) pipeline->reset_metrics();
    accounting_active = false;
    accounting_events = accounting_hits = accounting_input_ns = 0;
    run_started_at = std::chrono::steady_clock::now();
    publish_run_counters();
    run_active = true;
    cm_msg(MINFO, frontend_name, "Event-builder run is ready");
    return SUCCESS;
}

INT end_of_run(INT, char*) {
    run_active = false;
    std::lock_guard lock(pipeline_mutex);
    if (!drain_ring_before_transition()) {
        cm_msg(MERROR,frontend_name,"Cannot drain queued builder output before final window");
        return FE_ERR_HW;
    }
    if (pipeline) {
        const auto start = accounting_active ? accounting_start : std::chrono::steady_clock::now();
        const auto before = accounting_active ? accounting_before : pipeline->metrics();
        const auto hits_before = raw_input->hits_received();
        const bool had_pending = !pending_products.empty();
        for (const auto& event : pending_products)
            if (!send_from_transition(event)) return FE_ERR_HW;
        pending_products.clear();
        std::uint64_t input_events = 0;
        std::vector<std::uint8_t> input_event(static_cast<std::size_t>(max_event_size));
        const auto input_start = std::chrono::steady_clock::now();
        for (const auto& source : input_buffers) {
            while (true) {
                INT input_size = static_cast<INT>(input_event.size());
                const INT status = bm_receive_event(source.handle, input_event.data(), &input_size, BM_NO_WAIT);
                if (status == BM_ASYNC_RETURN) break;
                if (status != BM_SUCCESS || !ingest_input_event(reinterpret_cast<EVENT_HEADER*>(input_event.data())+1))
                    return FE_ERR_HW;
                ++input_events;
            }
        }
        const auto input_ns = elapsed_ns(input_start);
        auto events = pipeline->drain_and_flush();
        if (accounting_events || input_events || !events.empty() || had_pending ||
            pipeline->metrics().rejected_events!=before.rejected_events) {
            auto cycle = cycle_record(start, before, accounting_events + input_events,
                accounting_hits + raw_input->hits_received()-hits_before,
                accounting_input_ns + input_ns, events.size());
            const auto output_start = std::chrono::steady_clock::now();
            for (const auto& event : events) if (!send_from_transition(event)) return FE_ERR_HW;
            cycle.output_wait_ns = elapsed_ns(output_start);
            cycle.total_ns = elapsed_ns(start); cycle.flags = 1;
            if (!send_from_transition(diagnostic_event(cycle))) return FE_ERR_HW;
        }
    }
    accounting_active = false;
    accounting_events = accounting_hits = accounting_input_ns = 0;
    publish_run_counters();
    cm_msg(MINFO, frontend_name, "Run totals: input banks=%llu, input hits=%llu, output products=%llu, diagnostic events=%llu",
        static_cast<unsigned long long>(raw_input ? raw_input->banks_received() : 0),
        static_cast<unsigned long long>(raw_input ? raw_input->hits_received() : 0),
        static_cast<unsigned long long>(products_written.load()),
        static_cast<unsigned long long>(diagnostic_events_written.load()));
    return SUCCESS;
}

INT pause_run(INT, char*) { return SUCCESS; }
INT resume_run(INT, char*) { return SUCCESS; }

INT frontend_loop() {
    return SUCCESS;
}

INT poll_event(INT, INT, BOOL test) {
    return test ? FALSE : 0;
}

INT interrupt_configure(INT, INT, POINTER_T) { return SUCCESS; }

