#include "mfe.h"

#include "residual_compression/cpu_rice_golomb_codec.h"
#include "sampic_event_builder/pipeline/event_builder_pipeline.h"
#include "sampic_event_builder/stages/building/time_window_event_builder.h"
#include "sampic_event_builder/stages/compression/residual_compression_stage.h"
#include "sampic_event_builder/stages/compression/uncompressed_stage.h"
#include "sampic_event_builder/stages/finalization/product_bank_finalization_stage.h"
#include "sampic_event_builder/stages/fitting/passthrough_fitting_stage.h"
#include "sampic_event_builder/stages/fitting/template_fitting_stage.h"
#include "sampic_event_builder/stages/input/midas_raw_input_stage.h"
#include "sampic_event_builder/stages/selection/passthrough_selection_stage.h"
#include "sampic_event_builder/stages/selection/topology_selection_stage.h"
#include "template_fitter/cpu_least_squares_fitter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace sampic::event_builder;

const char* frontend_name = "SAMPIC Event Builder";
const char* frontend_file_name = __FILE__;
BOOL frontend_call_loop = FALSE;
// The curses-style MFE display busy-polls stdin when output is redirected
// (as in benchmarks/services), which can starve transition RPC handling.
INT display_period = 0;
INT max_event_size = 4 * 1024 * 1024;
INT max_event_size_frag = 0;
INT event_buffer_size = 64 * 1024 * 1024;
BOOL equipment_common_overwrite = FALSE;

// Implemented by MIDAS's ring-buffer support. Keep these narrow declarations
// instead of including msystem.h, whose C compatibility macros conflict with
// the C++ standard library used by the pipeline.
int rb_get_wp(int handle, void** pointer, int timeout_ms);
int rb_increment_wp(int handle, int size);

namespace {

constexpr short kInputEventId = 1;
constexpr char kInputBuffer[] = "SYSTEM";

HNDLE input_buffer = 0;
HNDLE input_request = 0;
MidasRawInputStage* raw_input = nullptr;
std::unique_ptr<waveform::fitting::CpuLeastSquaresFitter> fitter;
std::unique_ptr<waveform::compression::CpuRiceGolombCodec> codec;
std::unique_ptr<EventBuilderPipeline> pipeline;
std::thread pipeline_worker;
std::atomic<bool> worker_stop{false};
std::atomic<bool> run_active{false};
std::atomic<std::uint64_t> products_written{0};
std::atomic<std::uint64_t> product_bytes_written{0};
std::mutex pipeline_mutex;
std::vector<FinalizedEvent> pending_products;
std::chrono::steady_clock::time_point run_started_at;

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

waveform::fitting::CubicPulseTemplate pulse_template(std::size_t samples) {
    constexpr std::size_t density = 16;
    std::vector<double> result(samples * density);
    for (std::size_t index = 0; index < result.size(); ++index) {
        const double t = static_cast<double>(index) / density - 17.0;
        result[index] = t > 0.0
            ? std::exp(-t / 6.0) - std::exp(-t / 0.9) : 0.0;
    }
    return waveform::fitting::CubicPulseTemplate(std::move(result), density);
}

void ingest_input_event(void* event_payload) {
    if (raw_input == nullptr || event_payload == nullptr) {
        return;
    }

    BANK32* bank = nullptr;
    void* data = nullptr;
    while (const INT size = bk_iterate32(event_payload, &bank, &data)) {
        const std::string name(bank->name, sizeof(bank->name));
        if (name.size() != 4 || name[0] != 'S' || name[1] != 'R') {
            continue;
        }
        try {
            raw_input->ingest(
                name,
                {static_cast<const std::uint8_t*>(data),
                 static_cast<std::size_t>(size)});
        } catch (const std::exception& error) {
            cm_msg(MERROR, frontend_name,
                   "Rejected input bank %.4s: %s", name.c_str(), error.what());
        }
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
    bk_init32(payload);
    std::uint8_t* destination = nullptr;
    bk_create(payload, "SE00", TID_UINT8,
              reinterpret_cast<void**>(&destination));
    std::memcpy(destination, built.product_bank.data(), built.product_bank.size());
    bk_close(payload, destination + built.product_bank.size());
    event->data_size = bk_size(payload);

    const auto total_size = static_cast<int>(sizeof(EVENT_HEADER) + event->data_size);
    const INT commit = rb_increment_wp(ring_handle, total_size);
    if (commit != DB_SUCCESS) {
        cm_msg(MERROR, frontend_name,
               "Cannot commit event-builder ring event (%d)", commit);
        return false;
    }
    ++products_written;
    product_bytes_written += built.product_bank.size();
    return true;
}

bool send_from_transition(const FinalizedEvent& built) {
    std::vector<std::uint8_t> storage(
        sizeof(EVENT_HEADER) + static_cast<std::size_t>(max_event_size));
    auto* event = reinterpret_cast<EVENT_HEADER*>(storage.data());
    bm_compose_event_threadsafe(
        event, equipment[0].info.event_id, equipment[0].info.trigger_mask,
        0, &equipment[0].serial_number);
    auto* payload = reinterpret_cast<char*>(event + 1);
    bk_init32(payload);
    std::uint8_t* destination = nullptr;
    bk_create(payload, "SE00", TID_UINT8,
              reinterpret_cast<void**>(&destination));
    std::memcpy(destination, built.product_bank.data(), built.product_bank.size());
    bk_close(payload, destination + built.product_bank.size());
    event->data_size = bk_size(payload);

    const INT status = rpc_send_event(
        equipment[0].buffer_handle, event,
        static_cast<INT>(sizeof(EVENT_HEADER) + event->data_size),
        BM_WAIT, rpc_mode);
    if (status != BM_SUCCESS) {
        cm_msg(MERROR, frontend_name,
               "Cannot send flushed event-builder product (%d)", status);
        return false;
    }
    ++products_written;
    product_bytes_written += built.product_bank.size();
    return true;
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
    if (pipeline) {
        const auto& metrics = pipeline->metrics();
        const std::pair<const char*, const PipelineStageTotals*> stages[] = {
            {"Event building", &metrics.event_building},
            {"Selection", &metrics.selection},
            {"Fitting", &metrics.fitting},
            {"Compression", &metrics.compression},
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
        counters.emplace_back(
            "/Equipment/SAMPIC Builder/Variables/Fitting rejected",
            metrics.rejected_fits);
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
    std::vector<std::uint8_t> input_event(
        sizeof(EVENT_HEADER) + static_cast<std::size_t>(max_event_size));
    auto last_metrics_publish = std::chrono::steady_clock::now();

    while (is_readout_thread_enabled() && !worker_stop.load()) {
        if (!run_active.load() || !readout_enabled() || !pipeline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        std::lock_guard lock(pipeline_mutex);
        if (!run_active.load()) {
            continue;
        }
        bool received_input = false;
        for (std::size_t count = 0; count < max_input_batch; ++count) {
            INT input_size = static_cast<INT>(input_event.size());
            const INT receive_status = bm_receive_event(
                input_buffer, input_event.data(), &input_size, BM_NO_WAIT);
            if (receive_status == BM_ASYNC_RETURN) {
                break;
            }
            if (receive_status != BM_SUCCESS) {
                cm_msg(MERROR, frontend_name,
                       "Cannot receive SAMPIC input event (%d)", receive_status);
                break;
            }
            received_input = true;
            auto* header = reinterpret_cast<EVENT_HEADER*>(input_event.data());
            ingest_input_event(header + 1);
        }
        auto events = pipeline->run_once();
        const auto metrics_now = std::chrono::steady_clock::now();
        if (metrics_now - last_metrics_publish >= std::chrono::milliseconds(250)) {
            publish_run_counters();
            last_metrics_publish = metrics_now;
        }
        if (events.empty()) {
            if (!received_input) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            continue;
        }
        for (std::size_t index = 0; index < events.size(); ++index) {
            while (!worker_stop.load() && run_active.load() &&
                   readout_enabled() &&
                   !write_to_ring(events[index], ring_handle)) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            if (!run_active.load() || !readout_enabled()) {
                pending_products.insert(
                    pending_products.end(),
                    std::make_move_iterator(events.begin() + index),
                    std::make_move_iterator(events.end()));
                break;
            }
        }
    }
    signal_readout_thread_active(thread_index, FALSE);
}

} // namespace

INT frontend_init() {
    // The builder must subscribe before crate producers start and stop only
    // after those producers have flushed their final source banks.
    cm_set_transition_sequence(TR_START, 400);
    cm_set_transition_sequence(TR_STOP, 600);

    const auto window_ns = odb_value<INT>(
        "/Equipment/SAMPIC Builder/Settings/Event window ns", 200, TID_INT);
    const auto allowed_lateness_ns = odb_value<INT>(
        "/Equipment/SAMPIC Builder/Settings/Allowed lateness ns", 2000, TID_INT);
    const auto expected_crates = odb_value<INT>(
        "/Equipment/SAMPIC Builder/Settings/Expected crates", 1, TID_INT);
    const auto plane_count = odb_value<INT>(
        "/Equipment/SAMPIC Builder/Settings/Selection/Plane count", 8, TID_INT);
    const auto channels_per_plane = odb_value<INT>(
        "/Equipment/SAMPIC Builder/Settings/Selection/Channels per plane", 32, TID_INT);
    const auto maximum_punch_integral = odb_value<INT>(
        "/Equipment/SAMPIC Builder/Settings/Selection/Maximum punch integral per hit",
        3000, TID_INT);
    const BOOL selection_enabled = odb_value<BOOL>(
        "/Equipment/SAMPIC Builder/Settings/Selection/Enabled", FALSE, TID_BOOL);
    const BOOL fitting_enabled = odb_value<BOOL>(
        "/Equipment/SAMPIC Builder/Settings/Fitting/Enabled", FALSE, TID_BOOL);
    const double maximum_reduced_chi_squared = odb_value<double>(
        "/Equipment/SAMPIC Builder/Settings/Fitting/Maximum reduced chi squared",
        5.0, TID_DOUBLE);
    const double noise_sigma_adc = odb_value<double>(
        "/Equipment/SAMPIC Builder/Settings/Fitting/Noise sigma ADC", 3.0, TID_DOUBLE);
    const BOOL compression_enabled = odb_value<BOOL>(
        "/Equipment/SAMPIC Builder/Settings/Compression/Enabled", FALSE, TID_BOOL);
    const auto rice_k = odb_value<INT>(
        "/Equipment/SAMPIC Builder/Settings/Compression/Rice k", 4, TID_INT);
    const BOOL compress_rejected_fits = odb_value<BOOL>(
        "/Equipment/SAMPIC Builder/Settings/Compression/Compress rejected fits",
        FALSE, TID_BOOL);

    auto input = std::make_unique<MidasRawInputStage>();
    raw_input = input.get();

    std::unique_ptr<SelectionStage> selection;
    if (selection_enabled) {
        selection = std::make_unique<TopologyEventSelector>(TopologySelectorConfig{
            static_cast<std::size_t>(std::max(2, plane_count)),
            static_cast<std::size_t>(std::max(1, channels_per_plane)),
            maximum_punch_integral});
    } else {
        selection = std::make_unique<PassthroughSelectionStage>();
    }

    std::unique_ptr<FittingStage> fitting;
    if (fitting_enabled) {
        fitter = std::make_unique<waveform::fitting::CpuLeastSquaresFitter>(
            waveform::fitting::CpuLeastSquaresConfig{
                pulse_template(64), 4.0, 0.125, std::max(1e-6, noise_sigma_adc)});
        fitting = std::make_unique<TemplateFittingStage>(
            TemplateFittingStageConfig{1, maximum_reduced_chi_squared}, *fitter);
    } else {
        fitting = std::make_unique<PassthroughFittingStage>();
    }

    std::unique_ptr<CompressionStage> compression;
    if (compression_enabled) {
        codec = std::make_unique<waveform::compression::CpuRiceGolombCodec>(
            static_cast<std::uint8_t>(std::clamp(rice_k, 0, 31)));
        compression = std::make_unique<ResidualCompressionStage>(
            *codec, compress_rejected_fits != FALSE);
    } else {
        compression = std::make_unique<UncompressedStage>();
    }

    pipeline = std::make_unique<EventBuilderPipeline>(
        std::move(input), std::make_unique<TimeWindowEventBuilder>(
            TimeWindowEventBuilderConfig{
                static_cast<std::uint64_t>(std::max(1, window_ns)),
                static_cast<std::uint64_t>(std::max(0, allowed_lateness_ns)),
                static_cast<std::size_t>(std::max(1, expected_crates))}),
        std::move(selection), std::move(fitting), std::move(compression),
        std::make_unique<ProductBankFinalizationStage>());

    INT status = bm_open_buffer(kInputBuffer, 2 * max_event_size, &input_buffer);
    if (status != BM_SUCCESS && status != BM_CREATED) {
        cm_msg(MERROR, frontend_name, "Cannot open input buffer (%d)", status);
        return FE_ERR_HW;
    }
    status = bm_request_event(
        input_buffer, kInputEventId, TRIGGER_ALL, GET_ALL,
        &input_request, nullptr);
    if (status != BM_SUCCESS) {
        cm_msg(MERROR, frontend_name, "Cannot request input events (%d)", status);
        return FE_ERR_HW;
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
    if (input_request != 0) {
        bm_delete_request(input_request);
        input_request = 0;
    }
    if (input_buffer != 0) {
        bm_close_buffer(input_buffer);
        input_buffer = 0;
    }
    pipeline.reset();
    raw_input = nullptr;
    codec.reset();
    fitter.reset();
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
    if (pipeline) pipeline->reset_metrics();
    run_started_at = std::chrono::steady_clock::now();
    publish_run_counters();
    run_active = true;
    cm_msg(MINFO, frontend_name, "Event-builder run is ready");
    return SUCCESS;
}

INT end_of_run(INT, char*) {
    run_active = false;
    {
        std::lock_guard lock(pipeline_mutex);
        if (pipeline) {
            for (const auto& event : pending_products) {
                if (!send_from_transition(event)) {
                    return FE_ERR_HW;
                }
            }
            pending_products.clear();
            std::vector<std::uint8_t> input_event(
                sizeof(EVENT_HEADER) + static_cast<std::size_t>(max_event_size));
            while (true) {
                INT input_size = static_cast<INT>(input_event.size());
                const INT status = bm_receive_event(
                    input_buffer, input_event.data(), &input_size, BM_NO_WAIT);
                if (status == BM_ASYNC_RETURN) {
                    break;
                }
                if (status != BM_SUCCESS) {
                    cm_msg(MERROR, frontend_name,
                           "Cannot drain final SAMPIC input event (%d)", status);
                    return FE_ERR_HW;
                }
                auto* header = reinterpret_cast<EVENT_HEADER*>(input_event.data());
                ingest_input_event(header + 1);
            }
            for (const auto& event : pipeline->run_once()) {
                if (!send_from_transition(event)) {
                    return FE_ERR_HW;
                }
            }
            for (const auto& event : pipeline->flush()) {
                if (!send_from_transition(event)) {
                    return FE_ERR_HW;
                }
            }
        }
    }
    publish_run_counters();
    cm_msg(MINFO, frontend_name,
           "Run totals: input banks=%llu, input hits=%llu, output products=%llu, output bytes=%llu",
           static_cast<unsigned long long>(raw_input ? raw_input->banks_received() : 0),
           static_cast<unsigned long long>(raw_input ? raw_input->hits_received() : 0),
           static_cast<unsigned long long>(products_written.load()),
           static_cast<unsigned long long>(product_bytes_written.load()));
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

