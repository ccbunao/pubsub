// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <shm_pubsub/shm/publisher.h>
#include <shm_pubsub/shm_pubsub_logger.h>

#include "shm/shm_layout.h"
#include "shm/shm_region.h"

namespace shm_pubsub::shm
{
  class Publisher_Impl : public std::enable_shared_from_this<Publisher_Impl>
  {
  public:
    explicit Publisher_Impl(std::string topic_name, const PublisherOptions& options);
    ~Publisher_Impl();

    Publisher_Impl(const Publisher_Impl&)            = delete;
    Publisher_Impl& operator=(const Publisher_Impl&) = delete;
    Publisher_Impl(Publisher_Impl&&)                 = delete;
    Publisher_Impl& operator=(Publisher_Impl&&)      = delete;

    bool isRunning() const;
    bool send(const std::vector<std::pair<const char* const, const std::size_t>>& payloads);

  private:
    std::string format_prefix_() const;
    std::size_t compute_slot_count_() const;
    void log_drop_newest_(std::size_t message_size);
    void log_drop_oldest_(std::uint64_t dropped_messages);
    shm_pubsub::detail::SubscriberEntry* subscriber_table_();
    shm_pubsub::detail::SlotHeader* slot_header_at_(std::uint64_t msg_idx);
    char* slot_payload_at_(std::uint64_t msg_idx);

    std::string topic_name_;
    std::string region_name_;
    PublisherOptions options_;

    mutable std::mutex send_mutex_;
    shm_pubsub::detail::ShmRegion region_;
  };
}
