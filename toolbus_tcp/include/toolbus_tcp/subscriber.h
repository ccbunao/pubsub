#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <tcp_pubsub/executor.h>
#include <tcp_pubsub/subscriber.h>

namespace toolbus_tcp
{
  class Subscriber
  {
  public:
    using handler_t = std::function<void(const std::string& payload)>;

    Subscriber(const std::shared_ptr<tcp_pubsub::Executor>& executor,
               const std::string& host,
               std::uint16_t port);

    void on(const std::string& topic, handler_t handler);

    void cancel();

  private:
    void handle_raw_(const tcp_pubsub::CallbackData& data);

    tcp_pubsub::Subscriber subscriber_;

    std::mutex handlers_mutex_;
    std::unordered_map<std::string, handler_t> handlers_;
  };
}

