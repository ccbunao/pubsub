#include <toolbus_tcp/publisher.h>

#include <toolbus_tcp/mux.h>

namespace toolbus_tcp
{
  Publisher::Publisher(const std::shared_ptr<tcp_pubsub::Executor>& executor,
                       const std::string& bind_address,
                       std::uint16_t port)
    : publisher_(executor, bind_address, port)
  {}

  bool Publisher::isRunning() const { return publisher_.isRunning(); }

  bool Publisher::publish(const std::string& topic, const std::string& payload)
  {
    const std::string framed = encode_frame(topic, payload);
    if (framed.empty())
      return false;
    return publisher_.send(framed.data(), framed.size());
  }

  std::uint16_t Publisher::port() const { return publisher_.getPort(); }
}

