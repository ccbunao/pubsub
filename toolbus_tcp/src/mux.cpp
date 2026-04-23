#include <toolbus_tcp/mux.h>

#include <limits>

namespace toolbus_tcp
{
  static void write_u32be(std::string& out, std::uint32_t v)
  {
    out.push_back(static_cast<char>((v >> 24) & 0xFF));
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
    out.push_back(static_cast<char>(v & 0xFF));
  }

  static bool read_u32be(const char* data, std::size_t size, std::size_t& offset, std::uint32_t& out)
  {
    if (offset + 4 > size)
      return false;
    const auto b0 = static_cast<std::uint8_t>(data[offset + 0]);
    const auto b1 = static_cast<std::uint8_t>(data[offset + 1]);
    const auto b2 = static_cast<std::uint8_t>(data[offset + 2]);
    const auto b3 = static_cast<std::uint8_t>(data[offset + 3]);
    out = (static_cast<std::uint32_t>(b0) << 24) |
          (static_cast<std::uint32_t>(b1) << 16) |
          (static_cast<std::uint32_t>(b2) << 8) |
          (static_cast<std::uint32_t>(b3));
    offset += 4;
    return true;
  }

  std::string encode_frame(const std::string& topic, const std::string& payload)
  {
    if (topic.size() > std::numeric_limits<std::uint32_t>::max() ||
        payload.size() > std::numeric_limits<std::uint32_t>::max())
      return {};

    std::string out;
    out.reserve(8 + topic.size() + payload.size());
    write_u32be(out, static_cast<std::uint32_t>(topic.size()));
    write_u32be(out, static_cast<std::uint32_t>(payload.size()));
    out.append(topic);
    out.append(payload);
    return out;
  }

  bool decode_frame(const char* data, std::size_t size, MuxFrame& out)
  {
    std::size_t offset = 0;
    std::uint32_t topic_len = 0;
    std::uint32_t payload_len = 0;

    if (!read_u32be(data, size, offset, topic_len))
      return false;
    if (!read_u32be(data, size, offset, payload_len))
      return false;

    const std::size_t need = offset + static_cast<std::size_t>(topic_len) + static_cast<std::size_t>(payload_len);
    if (need != size)
      return false;

    out.topic.assign(data + offset, data + offset + topic_len);
    offset += topic_len;
    out.payload.assign(data + offset, data + offset + payload_len);
    return true;
  }
}

