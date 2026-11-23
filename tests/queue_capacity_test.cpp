#include "integration/sampic/collector/sampic_event_buffer.h"
#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include <stdexcept>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
int main() {
    for (std::size_t capacity : {1u, 10u, 11u}) {
        SampicEventBuffer input(capacity);
        FrontendEventBuffer output(capacity);
        auto first = std::make_shared<SampicEvent>();
        auto front = std::make_shared<FrontendEvent>();
        for (std::size_t i = 0; i < capacity; ++i) {
            input.push(first); output.push(front);
        }
        auto rejected = std::make_shared<SampicEvent>();
        auto rejected_front = std::make_shared<FrontendEvent>();
        bool failed = false;
        try { input.push(rejected); }
        catch (const std::overflow_error& error) {
            failed = std::string(error.what()).find("SAMPIC input") != std::string::npos &&
                     std::string(error.what()).find("capacity="+std::to_string(capacity)) != std::string::npos;
        }
        check(failed, "input overflow must immediately report queue and capacity");
        failed = false;
        try { output.push(rejected_front); }
        catch (const std::overflow_error& error) { failed = std::string(error.what()).find("Frontend output") != std::string::npos; }
        check(failed, "output overflow must immediately report queue");
        check(!output.tryPush(rejected_front), "tryPush must backpressure without throwing");
        for (std::size_t i = 0; i < capacity; ++i) {
            check(input.pop() == first, "input evicted queued data");
            check(output.pop().value() == front, "output evicted queued data");
        }
        check(input.empty() && output.empty(), "rejected events entered queue");
        input.push(rejected); output.push(rejected_front);
        check(input.pop() == rejected && output.pop().value() == rejected_front, "cannot drain after overflow");
    }
    FrontendEventBuffer restored(1);
    auto event = std::make_shared<FrontendEvent>();
    restored.push(event); restored.pushFront(event);
    check(restored.size() == 2, "restoring an in-flight event must preserve it");
    check(restored.pop().value() == event && restored.pop().value() == event, "restore lost event");
    bool invalid = false;
    try { SampicEventBuffer zero(0); } catch(const std::invalid_argument&) { invalid = true; }
    check(invalid, "zero input capacity accepted");
    invalid = false;
    try { FrontendEventBuffer zero(0); } catch(const std::invalid_argument&) { invalid = true; }
    check(invalid, "zero output capacity accepted");
    // Concurrent producers all observe overflow without evicting accepted data.
    SampicEventBuffer concurrent(10);
    std::atomic<int> rejected_count{0};
    std::vector<std::thread> producers;
    for (int i=0; i<4; ++i) producers.emplace_back([&] {
        for (int j=0; j<100; ++j) {
            try { concurrent.push(std::make_shared<SampicEvent>()); }
            catch (const std::overflow_error&) { ++rejected_count; }
        }
    });
    for (auto& thread:producers) thread.join();
    check(concurrent.size() == 10 && rejected_count == 390, "concurrent overflow handling");
}
