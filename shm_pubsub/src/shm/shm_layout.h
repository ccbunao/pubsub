// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#pragma once

#include <cstddef>
#include <cstdint>

namespace shm_pubsub::detail
{
  static constexpr std::uint32_t k_magic   = 0x53484D50U; // "SHMP"
  static constexpr std::uint16_t k_version = 2;

  struct alignas(8) ShmHeader
  {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t header_size;
    std::uint64_t region_size_bytes;

    // Ring configuration
    std::uint64_t slot_size_bytes;
    std::uint64_t slot_count;
    std::uint64_t max_subscribers;

    // Ring state
    volatile std::uint64_t write_idx;

    // Statistics (best-effort, monotonically increasing)
    volatile std::uint64_t dropped_newest_count;
    volatile std::uint64_t dropped_oldest_count;

    std::uint64_t _reserved[4];
  };

  struct alignas(8) SubscriberEntry
  {
    // 0 = free, 1 = active
    volatile std::uint64_t active;
    volatile std::uint64_t token;

    // Monotonic message index of last-consumed message
    volatile std::uint64_t read_idx;

    // Number of messages this subscriber has lost due to overwrite / policy
    volatile std::uint64_t lost_count;

    // Best-effort liveness hint (not yet used for GC)
    volatile std::uint64_t last_seen_ns;
  };

  struct alignas(8) SlotHeader
  {
    // seqlock counter, even == stable
    volatile std::uint64_t seq;

    // Monotonic index assigned by publisher
    std::uint64_t msg_idx;

    // payload size in bytes
    volatile std::uint64_t size;
  };

  inline std::size_t subscriber_table_offset_bytes()
  {
    return sizeof(ShmHeader);
  }

  inline std::size_t ring_offset_bytes(std::size_t max_subscribers)
  {
    return sizeof(ShmHeader) + max_subscribers * sizeof(SubscriberEntry);
  }

  inline std::size_t slot_stride_bytes(std::size_t slot_size_bytes)
  {
    return sizeof(SlotHeader) + slot_size_bytes;
  }

  inline std::size_t ring_bytes(std::size_t slot_count, std::size_t slot_size_bytes)
  {
    return slot_count * slot_stride_bytes(slot_size_bytes);
  }

  inline std::size_t total_region_size(std::size_t max_subscribers, std::size_t slot_count, std::size_t slot_size_bytes)
  {
    return ring_offset_bytes(max_subscribers) + ring_bytes(slot_count, slot_size_bytes);
  }
}
