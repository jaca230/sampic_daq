#include "processing/sampic_processing/collector/banks/frontend_event_hit_banks.h"
#include "processing/sampic_processing/collector/modes/default/default_config.h"
#include <algorithm>
#include <barrier>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

struct Result { std::uint64_t hits = 0, bytes = 0, checksum = 0; };

int main(int argc, char** argv) {
    const double seconds = argc > 1 ? std::stod(argv[1]) : 0.25;
    if (!(seconds > 0 && seconds <= 10)) return 2;
    // Larger than a tiny cache-resident event: rotate through independent decoded hits.
    std::vector<HitStruct> pool(65536);
    for (std::size_t i = 0; i < pool.size(); ++i) {
        auto& hit = pool[i];
        hit.FeBoardIndex = (i / 64) % 4; hit.Channel = i % 64;
        hit.SampicIndex = hit.Channel / 16; hit.ChannelIndex = hit.Channel % 16;
        hit.HitNumber = i; hit.DataSize = 64; hit.ADCCorrected = TRUE; hit.INLCorrected = TRUE;
        hit.CellInfo = i % 64; hit.FirstCellPhysicalIndex = (i + 7) % 64;
        hit.FirstCellTimeStamp = i * 1000.0; hit.RawTOTValue = 42; hit.TOTValue = 12.5;
        hit.AdvancedParams.TriggerPosition[i % 64] = TRUE;
        hit.AdvancedParams.PhysicalCell0TimeStamp = hit.FirstCellTimeStamp - 2;
        hit.AdvancedParams.FPGATimeStamp = i * 7;
        for (int s = 0; s < 64; ++s)
            hit.CorrectedDataSamples[s] = 0.2f + 0.1f * std::sin((s + i % 20) * 0.2f);
    }
    auto parent = std::make_shared<SampicEvent>();
    std::vector<std::shared_ptr<SampicEvent>> parents{parent};
    std::cout << "format,workers,hits_per_event,trial,hits_per_s,ns_per_hit,payload_MB_per_s,checksum\n";
    for (int workers : {1, 2}) for (int batch : {1, 32, 256, 1024}) for (bool verbose : {false, true}) {
        for (int trial = 1; trial <= 3; ++trial) {
            std::barrier start(workers + 1);
            std::vector<Result> results(workers);
            std::vector<std::thread> threads;
            for (int w = 0; w < workers; ++w) threads.emplace_back([&, w] {
                FrontendCollectorModeDefaultConfig config; config.verbose_banks = verbose;
                std::vector<const HitStruct*> hits(batch);
                std::vector<std::uint8_t> serialized(32 + 398 * batch);
                std::size_t index = w * pool.size() / workers;
                Result result;
                start.arrive_and_wait();
                const auto begin = std::chrono::steady_clock::now();
                do {
                    for (auto& hit : hits) { hit = &pool[index]; index = (index + 1) % pool.size(); }
                    FrontendEvent event;
                    addFrontendHitBanks(event, parents, hits, config);
                    std::size_t offset = 0;
                    for (const auto* bank : event.banks()) {
                        bank->writeTo(serialized.data() + offset);
                        offset += bank->size();
                    }
                    result.hits += batch; result.bytes += offset;
                    result.checksum += serialized[offset - 1] + serialized[offset / 2];
                } while (std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count() < seconds);
                results[w] = result;
            });
            const auto begin = std::chrono::steady_clock::now();
            start.arrive_and_wait();
            for (auto& thread : threads) thread.join();
            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
            Result total;
            for (const auto& r : results) { total.hits += r.hits; total.bytes += r.bytes; total.checksum += r.checksum; }
            const double rate = total.hits / elapsed;
            std::cout << (verbose ? "verbose" : "compact") << ',' << workers << ',' << batch << ',' << trial << ','
                      << std::fixed << std::setprecision(2) << rate << ',' << 1e9/rate << ','
                      << total.bytes/elapsed/1e6 << ',' << total.checksum << std::endl;
        }
    }
}
