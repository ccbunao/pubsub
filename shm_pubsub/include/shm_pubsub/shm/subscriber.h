// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <shm_pubsub/callback_data.h>
#include <shm_pubsub/shm_pubsub_export.h>

namespace shm_pubsub
{
  namespace shm
  {
    class Subscriber_Impl;

    class Subscriber
    {
    public:
      SHM_PUBSUB_EXPORT explicit Subscriber(const std::string& topic_name);

      SHM_PUBSUB_EXPORT Subscriber(const Subscriber&)            = default;
      SHM_PUBSUB_EXPORT Subscriber& operator=(const Subscriber&) = default;
      SHM_PUBSUB_EXPORT Subscriber(Subscriber&&) noexcept        = default;
      SHM_PUBSUB_EXPORT Subscriber& operator=(Subscriber&&) noexcept = default;

      SHM_PUBSUB_EXPORT ~Subscriber();

      SHM_PUBSUB_EXPORT bool isRunning() const;

      SHM_PUBSUB_EXPORT void setCallback(const std::function<void(const CallbackData&)>& callback_function, bool synchronous_execution = false);
      SHM_PUBSUB_EXPORT void clearCallback();

      SHM_PUBSUB_EXPORT void start(std::uint32_t poll_interval_ms = 2);
      SHM_PUBSUB_EXPORT void cancel();

    private:
      std::shared_ptr<Subscriber_Impl> subscriber_impl_;
    };
  }
}
