// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include <shm_pubsub/shm/subscriber.h>

#include <utility>

#include "subscriber_impl.h"

namespace shm_pubsub::shm
{
  Subscriber::Subscriber(const std::string& topic_name)
    : subscriber_impl_(std::make_shared<Subscriber_Impl>(topic_name))
  {}

  Subscriber::~Subscriber() = default;

  bool Subscriber::isRunning() const
  {
    return subscriber_impl_ && subscriber_impl_->isRunning();
  }

  void Subscriber::setCallback(const std::function<void(const CallbackData&)>& callback_function, bool synchronous_execution)
  {
    if (subscriber_impl_)
      subscriber_impl_->setCallback(callback_function, synchronous_execution);
  }

  void Subscriber::clearCallback()
  {
    if (subscriber_impl_)
      subscriber_impl_->clearCallback();
  }

  void Subscriber::start(std::uint32_t poll_interval_ms)
  {
    if (subscriber_impl_)
      subscriber_impl_->start(poll_interval_ms);
  }

  void Subscriber::cancel()
  {
    if (subscriber_impl_)
      subscriber_impl_->cancel();
  }
}
