#include "sampic_event_builder/serialization/midas_bank_writer.h"
#include <algorithm>
#include <cassert>
#include <vector>

using namespace sampic::event_builder;
int main() {
    constexpr std::size_t limit = 16 * 1024 * 1024;
    // Guard both sides of a full-size destination. On rejection, nothing changes.
    std::vector<char> storage(limit + 32, '\x5a');
    char* payload = storage.data() + 16 + sizeof(EVENT_HEADER);
    FinalizedEvent event;
    event.product_bank.resize(limit - 40, 0x31);
    auto size = write_midas_banks(payload, event, limit);
    assert(sizeof(EVENT_HEADER) + size == limit - 4);
    assert(std::all_of(storage.begin(), storage.begin()+16, [](char c) { return c=='\x5a'; }));
    assert(std::all_of(storage.end()-16, storage.end(), [](char c) { return c=='\x5a'; }));
    void* bank_data = nullptr;
    assert(bk_locate(payload, "BD00", &bank_data) == event.product_bank.size());
    assert(static_cast<unsigned char*>(bank_data)[0] == 0x31);

    auto rejected_without_writing = [&](const FinalizedEvent& input, std::size_t capacity) {
        std::fill(storage.begin(),storage.end(),'\x5a');
        bool rejected = false;
        try { write_midas_banks(payload,input,capacity); }
        catch (const std::overflow_error&) { rejected = true; }
        assert(rejected);
        assert(std::all_of(storage.begin(), storage.end(), [](char c) { return c=='\x5a'; }));
    };
    // Only framing/padding pushes this beyond the limit.
    event.product_bank.resize(limit - 32);
    rejected_without_writing(event,limit);
    event.product_bank.resize(limit);
    rejected_without_writing(event,limit);

    // Every bank is preflighted, even when earlier banks would fit.
    event.product_bank.clear();
    event.diagnostic_banks = {{"BT00",std::vector<std::uint8_t>(128)},
                              {"BT01",std::vector<std::uint8_t>(128)},
                              {"BC00",std::vector<std::uint8_t>(128)}};
    constexpr auto diagnostics_size = sizeof(EVENT_HEADER)+sizeof(BANK_HEADER)+3*(sizeof(BANK32)+128);
    assert(sizeof(EVENT_HEADER)+write_midas_banks(payload,event,diagnostics_size)==diagnostics_size);
    rejected_without_writing(event,diagnostics_size-1);

    event.diagnostic_banks.clear(); event.product_bank.resize(1);
    constexpr auto one_byte_size = sizeof(EVENT_HEADER)+sizeof(BANK_HEADER)+sizeof(BANK32)+8;
    assert(sizeof(EVENT_HEADER)+write_midas_banks(payload,event,one_byte_size)==one_byte_size);
    rejected_without_writing(event,one_byte_size-1);
    rejected_without_writing(event,sizeof(EVENT_HEADER));
}
