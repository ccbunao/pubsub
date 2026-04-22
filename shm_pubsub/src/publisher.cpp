// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include <shm_pubsub/shm/publisher.h>

#include <utility>

#include "publisher_impl.h"

namespace shm_pubsub::shm
{
  Publisher::Publisher(const std::string& topic_name, std::size_t capacity_bytes)
    : publisher_impl_(std::make_shared<Publisher_Impl>(topic_name, PublisherOptions{capacity_bytes}))
  {}

  Publisher::Publisher(const std::string& topic_name, const PublisherOptions& options)
    : publisher_impl_(std::make_shared<Publisher_Impl>(topic_name, options))
  {}

  Publisher::~Publisher() = default;

  bool Publisher::isRunning() const
  {
    return publisher_impl_ && publisher_impl_->isRunning();
  }

  bool Publisher::send(const char* data, std::size_t size) const
  {
    return send({{data, size}});
  }

  bool Publisher::send(const std::vector<std::pair<const char* const, const std::size_t>>& payloads) const
  {
    if (!publisher_impl_)
      return false;
    return publisher_impl_->send(payloads);
  }
}
