#include <toolbus/dedup_cache.h>

#include <chrono>

namespace toolbus
{
  static std::uint64_t now_ms()
  {
    return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
  }

  DedupCache::DedupCache(std::uint32_t ttl_ms) : ttl_ms_(ttl_ms) {}

  bool DedupCache::get(const std::string& key, std::string& out_json)
  {
    const auto it = map_.find(key);
    if (it == map_.end())
      return false;
    const std::uint64_t t = now_ms();
    if (it->second.expires_ms <= t)
    {
      map_.erase(it);
      return false;
    }
    out_json = it->second.value_json;
    return true;
  }

  void DedupCache::put(const std::string& key, std::string value_json)
  {
    const std::uint64_t t = now_ms();
    map_[key] = Entry{t + ttl_ms_, std::move(value_json)};
  }

  void DedupCache::prune()
  {
    const std::uint64_t t = now_ms();
    if (t - last_prune_ms_ < 5000)
      return;
    last_prune_ms_ = t;

    for (auto it = map_.begin(); it != map_.end();)
    {
      if (it->second.expires_ms <= t)
        it = map_.erase(it);
      else
        ++it;
    }
  }
}

