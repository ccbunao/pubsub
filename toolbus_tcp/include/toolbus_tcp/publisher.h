#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <tcp_pubsub/executor.h>
#include <tcp_pubsub/publisher.h>

namespace toolbus_tcp
{
  class Publisher
  {
  public:
    Publisher(const std::shared_ptr<tcp_pubsub::Executor>& executor,
              const std::string& bind_address,
              std::uint16_t port);

    bool isRunning() const;

    // Publishes a message on a logical topic over a single TCP publisher.
    bool publish(const std::string& topic, const std::string& payload);

    std::uint16_t port() const;

  private:
    tcp_pubsub::Publisher publisher_;
  };
}

