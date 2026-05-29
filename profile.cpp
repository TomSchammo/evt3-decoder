// Minimal driver for profiling ts::decoder::CDDecoder against a raw EVT3 file.
//
// Opens the file through the Metavision HAL (same path as the bench harness),
// feeds the raw byte buffers into the decoder, and reports the event count and
// decode time.
//
//   ./profile_main <path/to/file.raw>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

// Output iterator that discards the events and just counts them. Lets us
// profile the decoder without paying for (or running out of memory on) the
// billions of events a multi-GB recording expands into.
template <typename T> struct CountingIterator {
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using reference = void;

  std::size_t *count;

  CountingIterator &operator=(const T &) {
    ++*count;
    return *this;
  }
  CountingIterator &operator*() { return *this; }
  CountingIterator &operator++() { return *this; }
  CountingIterator operator++(int) { return *this; }
};

} // namespace

#include <metavision/hal/device/device.h>
#include <metavision/hal/device/device_discovery.h>
#include <metavision/hal/facilities/i_events_stream.h>
#include <metavision/sdk/base/events/event_cd.h>

#include <ts/decoder.hpp>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <path/to/file.raw>\n", argv[0]);
    return 1;
  }

  const std::filesystem::path path{argv[1]};

  auto device = Metavision::DeviceDiscovery::open_raw_file(path.string());
  if (!device)
    throw std::runtime_error("Failed to open RAW file: " + path.string());

  auto event_stream = device->get_facility<Metavision::I_EventsStream>();
  if (!event_stream)
    throw std::runtime_error("Missing I_EventsStream facility");

  std::size_t event_count = 0;
  std::size_t byte_count = 0;

  ts::decoder::CDDecoder<Metavision::EventCD> decoder;

  event_stream->start();

  const auto start = std::chrono::steady_clock::now();

  while (true) {
    const short ret = event_stream->poll_buffer();
    if (ret < 0)
      break;

    const auto raw_data = event_stream->get_latest_raw_data();
    if (raw_data) {
      byte_count += raw_data.size();
      decoder.decode({raw_data.data(), raw_data.size()},
                     CountingIterator<Metavision::EventCD>{&event_count});
    }
  }

  const auto end = std::chrono::steady_clock::now();

  event_stream->stop();
  decoder.stop();

  const double secs = std::chrono::duration<double>(end - start).count();

  std::printf("file: %s\n", path.string().c_str());
  std::printf("events decoded: %zu\n", event_count);
  std::printf("decode time: %.3f s\n", secs);
  std::printf("throughput: %.1f Mevt/s, %.1f MB/s\n",
              static_cast<double>(event_count) / secs / 1e6,
              static_cast<double>(byte_count) / secs / 1e6);

  return 0;
}
