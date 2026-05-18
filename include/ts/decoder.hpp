#ifndef TS_DECODER_HPP
#define TS_DECODER_HPP

#include "packets.hpp"
#include <atomic>
#include <bit>
#include <chrono>
#include <cstring>
#include <iterator>
#include <print>
#include <span>
#include <thread>
#include <utilix/types.hpp>

namespace ts {
namespace decoder {

using namespace utilix::types;

template <typename T>
concept Int = NumericInt<std::remove_cvref_t<T>>;

template <typename T>
concept EventStruct = requires(T e) {
  { e.x } -> Int;
  { e.y } -> Int;
  { e.t } -> Int;
  { e.p } -> Int;
};

struct Event {
  u32 t;
  u16 x, y;
  u8 p;
};

static_assert(sizeof(Event) == 96 / 8, "expected size to be 96 bit (last 8 padded to 32)");

// 49 bit + 15 bit padding
struct EventBP {
  u32 t : 24;
  u32 x : 12;
  u32 y : 12;
  u32 p : 1;
};

static_assert(sizeof(EventBP) == 64 / 8,
              "expected size to be 64 bit (padded added 15 bit padding to the "
              "next 64 bit boundary)");

// 57 bit + 7 bit padding
struct EventBPExt {
  u32 t : 32;
  u32 x : 12;
  u32 y : 12;
  u32 p : 1;
};
static_assert(sizeof(EventBPExt) == 64 / 8,
              "expected size to be 64 bit (padded added 7 bit padding to the "
              "next 64 bit boundary)");

// 64 bit + 0 bit padding
struct EventBPSat {
  u64 t : 39;
  u64 x : 12;
  u64 y : 12;
  u64 p : 1;
};
static_assert(sizeof(EventBPSat) == 64 / 8, "expected size to be 64 bit (no padding)");

// NOTE: data is in little endian
static_assert(std::endian::native == std::endian::little,
              "assummed little endian but that is not true");

inline constexpr evt3::EventType peek_header(u8 msb) {
  return static_cast<evt3::EventType>((msb & 0xf0) >> 4);
}

template <EventStruct T> class CDDecoder {
public:
  CDDecoder() {
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
  CDDecoder(CDDecoder &&) = delete;
  CDDecoder(const CDDecoder &) = delete;
  CDDecoder &operator=(CDDecoder &&) = delete;
  CDDecoder &operator=(const CDDecoder &) = delete;
  ~CDDecoder() {}
  void stop() { error_thread.request_stop(); }
  template <std::output_iterator<T> It> constexpr It decode(std::span<const u8> data, It out) {

    for (size_t i = 0; i < data.size(); i += 2) {

      u16 word{};
      std::memcpy(&word, data.data() + i, sizeof(word));

      switch (peek_header(data.data()[i + 1])) {
      case evt3::EventType::EVT_ADDR_Y: {
        const auto p = std::bit_cast<evt3::EvtAddrY>(word);
        cur.y = p.y;
        break;
      }
      case evt3::EventType::EVT_ADDR_X: {
        const auto p = std::bit_cast<evt3::EvtAddrX>(word);
        cur.x = p.x;
        cur.p = p.pol;
        *out = cur;
        out++;
        break;
      }
      case evt3::EventType::VECT_BASE_X: {
        const auto p = std::bit_cast<evt3::VectBaseX>(word);
        cur.p = p.pol;
        x_base = p.x;
        break;
      }
      case evt3::EventType::VECT_12: {
        const auto p = std::bit_cast<evt3::Vect12>(word);
        u16 bit_vector = p.valid;
        while (bit_vector) {
          const u16 offset = static_cast<u16>(std::countr_zero(bit_vector));

          cur.x = x_base + offset;
          *out = cur;
          out++;
          bit_vector &= bit_vector - 1;
        }
        x_base += 12;
        break;
      }
      case evt3::EventType::VECT_8: {
        const auto p = std::bit_cast<evt3::Vect8>(word);
        u16 bit_vector = p.valid;
        while (bit_vector) {
          const u16 offset = static_cast<u16>(std::countr_zero(bit_vector));

          cur.x = x_base + offset;
          *out = cur;
          out++;
          bit_vector &= bit_vector - 1;
        }
        x_base += 8;
        break;
      }
      case evt3::EventType::EVT_TIME_LOW: {
        const auto p = std::bit_cast<evt3::EvtTimeLow>(word);
        cur.t &= ~static_cast<decltype(cur.t)>(0xfff);
        cur.t |= p.evt_time_low;

        if (p.evt_time_low < last_t_low) {
          if ((last_t_low - p.evt_time_low) < 4000) {
            low_packets_lost.fetch_add(1, std::memory_order_relaxed);
          }
        }

        last_t_low = p.evt_time_low;
        break;
      }
      case evt3::EventType::EVT_TIME_HIGH: {
        const auto p = std::bit_cast<evt3::EvtTimeHigh>(word);
        if (p.evt_time_high < last_t_high) {
          u16 diff = last_t_high - p.evt_time_high;

          // NOTE(tom): resolution is 4096, this includes margin of 96
          if (diff >= 4000) {
            wrap_around_counter++;
            cur.t &= 0xffffff;
            cur.t |= (static_cast<decltype(cur.t)>(wrap_around_counter) << 24);
          } else {
            high_packets_lost.fetch_add(1, std::memory_order_relaxed);
          }
        }
        last_t_high = p.evt_time_high;

        cur.t &= ~(static_cast<decltype(cur.t)>(0xfff) << 12);
        cur.t |= (static_cast<decltype(cur.t)>(p.evt_time_high) << 12);
        break;
      }
      case evt3::EventType::CONTINUED_4: {
        // TODO(tom): no cd event, skipping
        break;
      }
      case evt3::EventType::EXT_TRIGGER: {
        // TODO(tom): no cd event, skipping
        break;
      }
      case evt3::EventType::OTHERS: {
        // TODO(tom): no cd event, skipping
        break;
      }
      case evt3::EventType::CONTINUED_12: {
        // TODO(tom): no cd event, skipping
        break;
      }
      }
    }
    return out;
  }

private:
  u16 last_t_high = 0;
  u16 last_t_low = 0;

  T cur{};
  u16 x_base = 0;

  std::atomic<u32> high_packets_lost{0};
  std::atomic<u32> low_packets_lost{0};

  u32 wrap_around_counter = 0;
  std::jthread error_thread, decoder_thread;
};
} // namespace decoder
} // namespace ts

#endif // TS_DECODER_HPP
