#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <shm_pubsub/shm/publisher.h>

namespace toolbus_shm
{
  // A tiny topic->shm region publisher cache.
  class Publisher
  {
  public:
    explicit Publisher(std::size_t capacity_bytes = 4U * 1024U * 1024U);

    bool publish(const std::string& topic, const std::string& payload);

  private:
    std::size_t capacity_bytes_;
    std::mutex mutex_;
    std::unordered_map<std::string, shm_pubsub::shm::Publisher> publishers_;
  };
}

