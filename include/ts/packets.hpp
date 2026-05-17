#ifndef TS_PACKETS_HPP
#define TS_PACKETS_HPP

#include <bit>
#include <cstddef>
#include <type_traits>
#include <utilix/types.hpp>

namespace ts {
namespace evt3 {

using namespace utilix::types;

enum class EventType : u16 {
  EVT_ADDR_Y = 0b0000,
  EVT_ADDR_X = 0b0010,
  VECT_BASE_X = 0b0011,
  VECT_12 = 0b0100,
  VECT_8 = 0b0101,
  EVT_TIME_LOW = 0b0110,
  CONTINUED_4 = 0b0111,
  EVT_TIME_HIGH = 0b1000,
  EXT_TRIGGER = 0b1010,
  OTHERS = 0b1110,
  CONTINUED_12 = 0b1111,
};

// Bit fields are declared LSB-first to match the spec's bit ranges on
// little-endian targets. The 4-bit `type` field always occupies bits [15:12].

struct EvtAddrY {
  u16 y : 11;          // [10:0]
  u16 system_type : 1; // [11]    0=Master, 1=Slave
  u16 type : 4;        // [15:12] = 0b0000
};

struct EvtAddrX {
  u16 x : 11;   // [10:0]
  u16 pol : 1;  // [11]    0=OFF, 1=ON
  u16 type : 4; // [15:12] = 0b0010
};

struct VectBaseX {
  u16 x : 11;   // [10:0]  base x for following VECT_12 / VECT_8
  u16 pol : 1;  // [11]    0=OFF, 1=ON
  u16 type : 4; // [15:12] = 0b0011
};

struct Vect12 {
  u16 valid : 12; // [11:0]  validity bitmap for 12 consecutive events
  u16 type : 4;   // [15:12] = 0b0100
};

struct Vect8 {
  u16 valid : 8;  // [7:0]   validity bitmap for 8 consecutive events
  u16 unused : 4; // [11:8]
  u16 type : 4;   // [15:12] = 0b0101
};

struct EvtTimeLow {
  u16 evt_time_low : 12; // [11:0]  lower 12 bits of 24-bit timestamp
  u16 type : 4;          // [15:12] = 0b0110
};

struct Continued4 {
  u16 data : 4;   // [3:0]   context-dependent payload
  u16 unused : 8; // [11:4]
  u16 type : 4;   // [15:12] = 0b0111
};

struct EvtTimeHigh {
  u16 evt_time_high : 12; // [11:0]  upper 12 bits of 24-bit timestamp
  u16 type : 4;           // [15:12] = 0b1000
};

struct ExtTrigger {
  u16 value : 1;  // [0]     0=falling edge, 1=rising edge
  u16 unused : 7; // [7:1]
  u16 id : 4;     // [11:8]  trigger channel identifier
  u16 type : 4;   // [15:12] = 0b1010
};

struct Others {
  u16 subtype : 12; // [11:0]  extended event type identifier
  u16 type : 4;     // [15:12] = 0b1110
};

struct Continued12 {
  u16 data : 12; // [11:0]  context-dependent payload
  u16 type : 4;  // [15:12] = 0b1111
};

// struct GenericPacket {
//   u16 rest : 12;
//   u16 type : 4; // [15:12]
// };

// struct Packet {
// union Packet {
//   EvtAddrY y_addr;
//   EvtAddrX x_addr;
//   VectBaseX vb_x;
//   Vect12 v12;
//   Vect8 v8;
//   EvtTimeLow t_low;
//   Continued4 c4;
//   EvtTimeHigh t_high;
//   ExtTrigger trig;
//   Others others;
//   Continued12 c12;
//   GenericPacket p;
//   std::array<std::byte, 2> raw;
// };

//   uint16_t get_header() const { return others.type; }
// };

template <typename T> consteval bool check_packet() {
  static_assert(sizeof(T) == 2, "packet must be 16 bits");
  static_assert(std::is_trivially_copyable_v<T>, "packet must be trivially copyable");
  T p{};
  p.type = 0xF;
  return std::bit_cast<u16>(p) == 0xF000;
}

static_assert(check_packet<EvtAddrY>(), "EvtAddrY: type field not in [15:12]");
static_assert(check_packet<EvtAddrX>(), "EvtAddrX: type field not in [15:12]");
static_assert(check_packet<VectBaseX>(), "VectBaseX: type field not in [15:12]");
static_assert(check_packet<Vect12>(), "Vect12: type field not in [15:12]");
static_assert(check_packet<Vect8>(), "Vect8: type field not in [15:12]");
static_assert(check_packet<EvtTimeLow>(), "EvtTimeLow: type field not in [15:12]");
static_assert(check_packet<Continued4>(), "Continued4: type field not in [15:12]");
static_assert(check_packet<EvtTimeHigh>(), "EvtTimeHigh: type field not in [15:12]");
static_assert(check_packet<ExtTrigger>(), "ExtTrigger: type field not in [15:12]");
static_assert(check_packet<Others>(), "Others: type field not in [15:12]");
static_assert(check_packet<Continued12>(), "Continued12: type field not in [15:12]");

} // namespace evt3
} // namespace ts

#endif // TS_PACKETS_HPP
