#include <toolbus/id.h>

#include <array>
#include <cstdint>
#include <random>

namespace toolbus
{
  static char hex_nibble(std::uint8_t v)
  {
    return static_cast<char>((v < 10) ? ('0' + v) : ('a' + (v - 10)));
  }

  std::string make_call_id()
  {
    std::array<std::uint8_t, 16> bytes{};
    std::random_device rd;
    for (auto& b : bytes)
      b = static_cast<std::uint8_t>(rd());

    std::string out;
    out.resize(32);
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
      out[i * 2 + 0] = hex_nibble(static_cast<std::uint8_t>((bytes[i] >> 4) & 0xF));
      out[i * 2 + 1] = hex_nibble(static_cast<std::uint8_t>(bytes[i] & 0xF));
    }
    return out;
  }
}

