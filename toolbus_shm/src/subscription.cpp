#include <toolbus_shm/subscription.h>

namespace toolbus_shm
{
  void SubscriptionSet::subscribe(const std::string& topic, handler_t handler, std::uint32_t poll_interval_ms)
  {
    auto item = std::make_unique<Item>(Item{shm_pubsub::shm::Subscriber(topic)});
    item->sub.setCallback(
      [handler = std::move(handler)](const shm_pubsub::CallbackData& d)
      {
        if (!d.buffer_ || d.buffer_->empty())
          return;
        handler(std::string(d.buffer_->data(), d.buffer_->size()));
      },
      true);
    item->sub.start(poll_interval_ms);
    items_.push_back(std::move(item));
  }

  void SubscriptionSet::cancel_all()
  {
    for (auto& it : items_)
      it->sub.cancel();
    items_.clear();
  }
}

