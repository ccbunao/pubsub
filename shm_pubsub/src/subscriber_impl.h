// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>
#include <thread>
#include <vector>
#include <deque>

#include <shm_pubsub/callback_data.h>
#include <shm_pubsub/shm_pubsub_logger.h>

#include "shm/shm_layout.h"
#include "shm/shm_region.h"

namespace shm_pubsub::shm
{
  class Subscriber_Impl : public std::enable_shared_from_this<Subscriber_Impl>
  {
  public:
    explicit Subscriber_Impl(std::string topic_name);
    ~Subscriber_Impl();

    Subscriber_Impl(const Subscriber_Impl&)            = delete;
    Subscriber_Impl& operator=(const Subscriber_Impl&) = delete;
    Subscriber_Impl(Subscriber_Impl&&)                 = delete;
    Subscriber_Impl& operator=(Subscriber_Impl&&)      = delete;

    bool isRunning() const;

    void setCallback(const std::function<void(const CallbackData&)>& callback_function, bool synchronous_execution);
    void clearCallback();

    void start(std::uint32_t poll_interval_ms);
    void cancel();

  private:
    std::string format_prefix_() const;
    bool ensure_registered_();
    void unregister_if_needed_();
    shm_pubsub::detail::SubscriberEntry* subscriber_entry_();
    shm_pubsub::detail::SlotHeader* slot_header_at_(std::uint64_t msg_idx);
    const char* slot_payload_at_(std::uint64_t msg_idx);
    void log_lost_(std::uint64_t lost);

    void stopCallbackThreadIfNeeded();
    void pollLoop();
    void callbackLoop(std::function<void(const CallbackData&)> callback_function);

    std::string topic_name_;
    std::string region_name_;

    std::atomic<bool> running_;
    std::atomic<bool> canceled_;
    std::uint32_t poll_interval_ms_;

    shm_pubsub::detail::ShmRegion region_;
    std::uint64_t last_read_idx_;
    std::uint64_t subscriber_token_;
    std::int32_t subscriber_index_;
    shm_pubsub::logger::logger_t logger_;

    std::atomic<bool> user_callback_is_synchronous_;
    std::function<void(const CallbackData&)> synchronous_user_callback_;

    std::mutex last_callback_data_mutex_;
    std::condition_variable last_callback_data_cv_;
    std::deque<CallbackData> callback_queue_;
    std::size_t callback_queue_depth_;

    std::unique_ptr<std::thread> poll_thread_;
    std::unique_ptr<std::thread> callback_thread_;
    std::atomic<bool> callback_thread_stop_;
  };
}
