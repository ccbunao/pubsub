// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <shm_pubsub/shm_pubsub_export.h>
#include <shm_pubsub/shm_pubsub_logger.h>

namespace shm_pubsub
{
  namespace shm
  {
    class Publisher_Impl;

    enum class OverflowPolicy
    {
      // Keep older messages, drop new publishes when full
      DropNewest,
      // Keep newer messages, advance lagging subscribers when full
      DropOldest,
    };

    struct PublisherOptions
    {
      std::size_t capacity_bytes     = 4U * 1024U * 1024U; // total ring bytes (slots + headers), excluding shm header/sub table
      std::size_t slot_size_bytes    = 64U * 1024U;        // max payload per message
      std::uint32_t max_subscribers  = 8;
      OverflowPolicy overflow_policy = OverflowPolicy::DropOldest;

      // If a subscriber stops without unregistering (crash / SIGKILL),
      // its slot may remain active and stall the ring. If enabled, the publisher
      // will ignore/GC subscribers whose last_seen is older than this threshold.
      std::uint32_t subscriber_timeout_ms = 2000;

      shm_pubsub::logger::logger_t logger = shm_pubsub::logger::default_logger;
    };

    class Publisher
    {
    public:
      SHM_PUBSUB_EXPORT explicit Publisher(const std::string& topic_name, std::size_t capacity_bytes = 4U * 1024U * 1024U);
      SHM_PUBSUB_EXPORT explicit Publisher(const std::string& topic_name, const PublisherOptions& options);

      SHM_PUBSUB_EXPORT Publisher(const Publisher&)            = default;
      SHM_PUBSUB_EXPORT Publisher& operator=(const Publisher&) = default;
      SHM_PUBSUB_EXPORT Publisher(Publisher&&) noexcept        = default;
      SHM_PUBSUB_EXPORT Publisher& operator=(Publisher&&) noexcept = default;

      SHM_PUBSUB_EXPORT ~Publisher();

      SHM_PUBSUB_EXPORT bool isRunning() const;

      SHM_PUBSUB_EXPORT bool send(const char* data, std::size_t size) const;
      SHM_PUBSUB_EXPORT bool send(const std::vector<std::pair<const char* const, const std::size_t>>& payloads) const;

    private:
      std::shared_ptr<Publisher_Impl> publisher_impl_;
    };
  }
}
