#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <shm_pubsub/shm/subscriber.h>

namespace toolbus_shm
{
  // Owns subscribers for topics. Each shm_pubsub subscriber is topic-specific.
  class SubscriptionSet
  {
  public:
    using handler_t = std::function<void(const std::string& payload)>;

    SubscriptionSet() = default;

    void subscribe(const std::string& topic, handler_t handler, std::uint32_t poll_interval_ms = 1);
    void cancel_all();

  private:
    struct Item
    {
      shm_pubsub::shm::Subscriber sub;
    };

    std::vector<std::unique_ptr<Item>> items_;
  };
}

