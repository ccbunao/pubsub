#include <toolbus_tcp/subscriber.h>

#include <toolbus_tcp/mux.h>

#include <iostream>

namespace toolbus_tcp
{
  Subscriber::Subscriber(const std::shared_ptr<tcp_pubsub::Executor>& executor,
                         const std::string& host,
                         std::uint16_t port)
    : subscriber_(executor)
  {
    subscriber_.setCallback([this](const tcp_pubsub::CallbackData& d) { handle_raw_(d); }, false);
    subscriber_.addSession(host, port);
  }

  void Subscriber::on(const std::string& topic, handler_t handler)
  {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[topic] = std::move(handler);
  }

  void Subscriber::cancel() { subscriber_.cancel(); }

  void Subscriber::handle_raw_(const tcp_pubsub::CallbackData& data)
  {
    if (!data.buffer_ || data.buffer_->empty())
      return;

    MuxFrame frame;
    if (!decode_frame(data.buffer_->data(), data.buffer_->size(), frame))
      return;

    handler_t handler;
    {
      std::lock_guard<std::mutex> lock(handlers_mutex_);
      const auto it = handlers_.find(frame.topic);
      if (it == handlers_.end())
        return;
      handler = it->second;
    }

    if (handler)
      handler(frame.payload);
  }
}

