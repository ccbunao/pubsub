#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace toolbus
{
  class DedupCache
  {
  public:
    explicit DedupCache(std::uint32_t ttl_ms = 5U * 60U * 1000U);

    // If present and not expired, returns true and sets out_json.
    bool get(const std::string& key, std::string& out_json);

    // Stores a JSON-encoded ToolResult under key.
    void put(const std::string& key, std::string value_json);

    // Best-effort prune.
    void prune();

  private:
    struct Entry
    {
      std::uint64_t expires_ms = 0;
      std::string value_json;
    };

    std::uint32_t ttl_ms_;
    std::unordered_map<std::string, Entry> map_;
    std::uint64_t last_prune_ms_ = 0;
  };
}

