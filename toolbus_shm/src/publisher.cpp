#include <toolbus_shm/publisher.h>

namespace toolbus_shm
{
  Publisher::Publisher(std::size_t capacity_bytes) : capacity_bytes_(capacity_bytes) {}

  bool Publisher::publish(const std::string& topic, const std::string& payload)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = publishers_.find(topic);
    if (it == publishers_.end())
      it = publishers_.emplace(topic, shm_pubsub::shm::Publisher(topic, capacity_bytes_)).first;
    if (!it->second.isRunning())
      return false;
    return it->second.send(payload.data(), payload.size());
  }
}

