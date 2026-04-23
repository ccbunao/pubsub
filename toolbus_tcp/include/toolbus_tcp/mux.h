#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace toolbus_tcp
{
  struct MuxFrame
  {
    std::string topic;
    std::string payload;
  };

  // Binary framing:
  //   u32be topic_len
  //   u32be payload_len
  //   topic bytes
  //   payload bytes
  std::string encode_frame(const std::string& topic, const std::string& payload);

  // Returns true on success and fills out. Throws only on programmer error (e.g. huge lengths).
  bool decode_frame(const char* data, std::size_t size, MuxFrame& out);
}

