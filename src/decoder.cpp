#include <chrono>
#include <print>
#include <ts/decoder.hpp>

using namespace ts::decoder;

CDDecoder::CDDecoder() {
  error_thread = std::jthread([this](std::stop_token st) {
    using namespace std::chrono_literals;
    while (!st.stop_requested()) {

      const u32 hpl = high_packets_lost.exchange(0, std::memory_order_relaxed);
      const u32 lpl = low_packets_lost.exchange(0, std::memory_order_relaxed);

      if (hpl)
        std::println("[WARN]: {} time_high packets have been dropped", hpl);

      if (lpl)
        std::println("[WARN]: {} time_low packets have been dropped", lpl);

      std::this_thread::sleep_for(std::chrono::nanoseconds(100ms));
    }
  });
}

CDDecoder::~CDDecoder() {}
void CDDecoder::stop() { error_thread.request_stop(); }
