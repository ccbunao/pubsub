// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include "publisher_impl.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <chrono>
#include <sstream>

#include "shm/atomic_u64.h"
#include "shm/shm_name.h"

namespace shm_pubsub::shm
{
  namespace
  {
    std::size_t clamp_min(std::size_t v, std::size_t min_v) { return v < min_v ? min_v : v; }

    std::uint64_t now_ns()
    {
      return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
    }
  }

  Publisher_Impl::Publisher_Impl(std::string topic_name, const PublisherOptions& options)
    : topic_name_(std::move(topic_name))
    , region_name_(shm_pubsub::detail::build_region_name(topic_name_))
    , options_(options)
  {
    if (options_.max_subscribers == 0)
      options_.max_subscribers = 1;

    options_.slot_size_bytes = clamp_min(options_.slot_size_bytes, 256U);
    const std::size_t slot_count = compute_slot_count_();
    const std::size_t region_size = shm_pubsub::detail::total_region_size(options_.max_subscribers, slot_count, options_.slot_size_bytes);

    region_ = shm_pubsub::detail::create_or_open(region_name_, region_size);
    if (region_.addr == nullptr)
      return;

    auto* header = static_cast<shm_pubsub::detail::ShmHeader*>(region_.addr);

    if (region_.owner)
    {
      std::memset(region_.addr, 0, region_size);
      header->magic             = shm_pubsub::detail::k_magic;
      header->version           = shm_pubsub::detail::k_version;
      header->header_size       = static_cast<std::uint16_t>(sizeof(shm_pubsub::detail::ShmHeader));
      header->region_size_bytes = static_cast<std::uint64_t>(region_size);
      header->slot_size_bytes   = static_cast<std::uint64_t>(options_.slot_size_bytes);
      header->slot_count        = static_cast<std::uint64_t>(slot_count);
      header->max_subscribers   = static_cast<std::uint64_t>(options_.max_subscribers);
      header->write_idx         = 0;
      header->dropped_newest_count = 0;
      header->dropped_oldest_count = 0;
    }
    else
    {
      const bool ok =
        (header->magic == shm_pubsub::detail::k_magic) &&
        (header->version == shm_pubsub::detail::k_version) &&
        (header->header_size == sizeof(shm_pubsub::detail::ShmHeader)) &&
        (header->region_size_bytes == static_cast<std::uint64_t>(region_size)) &&
        (header->slot_size_bytes == static_cast<std::uint64_t>(options_.slot_size_bytes)) &&
        (header->slot_count == static_cast<std::uint64_t>(slot_count)) &&
        (header->max_subscribers == static_cast<std::uint64_t>(options_.max_subscribers));

      if (!ok)
      {
        if (options_.logger)
        {
          std::ostringstream oss;
          oss << format_prefix_()
              << "shared memory exists but has incompatible layout/config. "
              << "Hint: stop all users and delete '/dev/shm/" << region_name_ << "' (Linux) or use a different topic. "
              << "Expected version=" << shm_pubsub::detail::k_version
              << " slot_size=" << options_.slot_size_bytes
              << " slot_count=" << slot_count
              << " max_subscribers=" << options_.max_subscribers
              << "; found version=" << header->version
              << " slot_size=" << header->slot_size_bytes
              << " slot_count=" << header->slot_count
              << " max_subscribers=" << header->max_subscribers;
          options_.logger(shm_pubsub::logger::LogLevel::Error, oss.str());
        }

        shm_pubsub::detail::close_region(region_);
        region_ = {};
      }
    }
  }

  Publisher_Impl::~Publisher_Impl()
  {
    shm_pubsub::detail::close_region(region_);
  }

  bool Publisher_Impl::isRunning() const
  {
    return region_.addr != nullptr;
  }

  std::string Publisher_Impl::format_prefix_() const
  {
    std::ostringstream oss;
    oss << "topic='" << topic_name_ << "': ";
    return oss.str();
  }

  std::size_t Publisher_Impl::compute_slot_count_() const
  {
    const std::size_t stride = shm_pubsub::detail::slot_stride_bytes(options_.slot_size_bytes);
    if (stride == 0)
      return 1;
    const std::size_t slots = options_.capacity_bytes / stride;
    return clamp_min(slots, 1U);
  }

  shm_pubsub::detail::SubscriberEntry* Publisher_Impl::subscriber_table_()
  {
    return reinterpret_cast<shm_pubsub::detail::SubscriberEntry*>(
      reinterpret_cast<char*>(region_.addr) + shm_pubsub::detail::subscriber_table_offset_bytes());
  }

  shm_pubsub::detail::SlotHeader* Publisher_Impl::slot_header_at_(std::uint64_t msg_idx)
  {
    auto* header = static_cast<shm_pubsub::detail::ShmHeader*>(region_.addr);
    const std::size_t slot_count = static_cast<std::size_t>(header->slot_count);
    const std::size_t slot_size  = static_cast<std::size_t>(header->slot_size_bytes);
    const std::size_t stride = shm_pubsub::detail::slot_stride_bytes(slot_size);
    const std::size_t ring_offset = shm_pubsub::detail::ring_offset_bytes(static_cast<std::size_t>(header->max_subscribers));

    const std::size_t slot_index = static_cast<std::size_t>(msg_idx % slot_count);
    char* slot_base = reinterpret_cast<char*>(region_.addr) + ring_offset + slot_index * stride;
    return reinterpret_cast<shm_pubsub::detail::SlotHeader*>(slot_base);
  }

  char* Publisher_Impl::slot_payload_at_(std::uint64_t msg_idx)
  {
    auto* slot = slot_header_at_(msg_idx);
    return reinterpret_cast<char*>(slot) + sizeof(shm_pubsub::detail::SlotHeader);
  }

  void Publisher_Impl::log_drop_newest_(std::size_t message_size)
  {
    if (options_.logger)
      options_.logger(shm_pubsub::logger::LogLevel::Warning, format_prefix_() + "ring full, drop newest publish (size=" + std::to_string(message_size) + ")");
  }

  void Publisher_Impl::log_drop_oldest_(std::uint64_t dropped_messages)
  {
    if (dropped_messages == 0)
      return;
    if (options_.logger)
      options_.logger(shm_pubsub::logger::LogLevel::Warning, format_prefix_() + "ring full, forced drop oldest for lagging subscribers (dropped=" + std::to_string(dropped_messages) + ")");
  }

  bool Publisher_Impl::send(const std::vector<std::pair<const char* const, const std::size_t>>& payloads)
  {
    if (!isRunning())
      return false;

    std::size_t total_size = 0;
    for (const auto& p : payloads)
      total_size += p.second;

    auto* header = static_cast<shm_pubsub::detail::ShmHeader*>(region_.addr);
    const std::size_t slot_size = static_cast<std::size_t>(header->slot_size_bytes);
    if (total_size > slot_size)
      return false;

    const std::lock_guard<std::mutex> lock(send_mutex_);

    volatile std::uint64_t* write_idx_ptr = &header->write_idx;
    const std::size_t slot_count = static_cast<std::size_t>(header->slot_count);

    // Determine min read idx across all subscribers (including inactive is fine, they are ignored)
    std::uint64_t min_read_idx = shm_pubsub::detail::load_u64_acquire(write_idx_ptr);
    const std::size_t max_subscribers = static_cast<std::size_t>(header->max_subscribers);
    auto* subs = subscriber_table_();
    const std::uint64_t tnow = now_ns();
    const std::uint64_t timeout_ns =
      options_.subscriber_timeout_ms ? (static_cast<std::uint64_t>(options_.subscriber_timeout_ms) * 1000ULL * 1000ULL) : 0ULL;
    for (std::size_t i = 0; i < max_subscribers; ++i)
    {
      if (shm_pubsub::detail::load_u64_acquire(&subs[i].active) == 0)
        continue;

      if (timeout_ns != 0)
      {
        const std::uint64_t last_seen = shm_pubsub::detail::load_u64_acquire(&subs[i].last_seen_ns);
        if (last_seen != 0 && (tnow > last_seen) && ((tnow - last_seen) > timeout_ns))
        {
          // stale subscriber (e.g., crashed without unregistering) -> GC
          subs[i].token = 0;
          shm_pubsub::detail::store_u64_release(&subs[i].active, 0);
          continue;
        }
      }

      const std::uint64_t r = shm_pubsub::detail::load_u64_acquire(&subs[i].read_idx);
      min_read_idx = std::min(min_read_idx, r);
    }

    const std::uint64_t write_idx = shm_pubsub::detail::load_u64_acquire(write_idx_ptr);
    const std::uint64_t next_idx = write_idx + 1;

    if ((next_idx - min_read_idx) > static_cast<std::uint64_t>(slot_count))
    {
      if (options_.overflow_policy == OverflowPolicy::DropNewest)
      {
        shm_pubsub::detail::fetch_add_u64_acq_rel(&header->dropped_newest_count, 1);
        log_drop_newest_(total_size);
        return false;
      }

      // DropOldest: advance lagging subscribers up to the new minimum allowed index
      const std::uint64_t new_min = next_idx - static_cast<std::uint64_t>(slot_count);
      std::uint64_t total_dropped = 0;
      for (std::size_t i = 0; i < max_subscribers; ++i)
      {
        if (shm_pubsub::detail::load_u64_acquire(&subs[i].active) == 0)
          continue;
        for (;;)
        {
          std::uint64_t r = shm_pubsub::detail::load_u64_acquire(&subs[i].read_idx);
          if (r >= new_min)
            break;

          std::uint64_t expected = r;
          if (shm_pubsub::detail::compare_exchange_u64_acq_rel(&subs[i].read_idx, expected, new_min))
          {
            const std::uint64_t lost = (new_min - r);
            shm_pubsub::detail::fetch_add_u64_acq_rel(&subs[i].lost_count, lost);
            total_dropped += lost;
            break;
          }
        }
      }
      if (total_dropped != 0)
      {
        shm_pubsub::detail::fetch_add_u64_acq_rel(&header->dropped_oldest_count, total_dropped);
        log_drop_oldest_(total_dropped);
      }
    }

    auto* slot = slot_header_at_(next_idx);
    volatile std::uint64_t* slot_seq_ptr  = &slot->seq;
    volatile std::uint64_t* slot_size_ptr = &slot->size;

    // Begin write: make slot seq odd
    const std::uint64_t s0 = shm_pubsub::detail::load_u64_acquire(slot_seq_ptr);
    if ((s0 % 2) == 0)
      shm_pubsub::detail::fetch_add_u64_acq_rel(slot_seq_ptr, 1);
    else
      shm_pubsub::detail::fetch_add_u64_acq_rel(slot_seq_ptr, 2);

    // Copy payload into shared memory
    char* payload_base = slot_payload_at_(next_idx);
    std::size_t offset = 0;
    for (const auto& p : payloads)
    {
      if (p.first != nullptr && p.second != 0)
      {
        std::memcpy(payload_base + offset, p.first, p.second);
        offset += p.second;
      }
    }

    slot->msg_idx = next_idx;
    shm_pubsub::detail::store_u64_release(slot_size_ptr, static_cast<std::uint64_t>(total_size));

    // End write: make slot seq even
    shm_pubsub::detail::fetch_add_u64_acq_rel(slot_seq_ptr, 1);

    // Publish by advancing write_idx after slot is stable
    shm_pubsub::detail::store_u64_release(write_idx_ptr, next_idx);

    return true;
  }
}
