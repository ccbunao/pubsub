// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include "subscriber_impl.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <random>
#include <sstream>
#include <thread>

#include "shm/atomic_u64.h"
#include "shm/shm_layout.h"
#include "shm/shm_name.h"

namespace shm_pubsub::shm
{
  namespace
  {
    std::uint64_t make_token()
    {
      std::random_device rd;
      const std::uint64_t a = (static_cast<std::uint64_t>(rd()) << 32) ^ static_cast<std::uint64_t>(rd());
      const std::uint64_t b = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
      return a ^ (b + 0x9e3779b97f4a7c15ULL);
    }

    std::uint64_t now_ns()
    {
      return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
    }
  }

  Subscriber_Impl::Subscriber_Impl(std::string topic_name)
    : topic_name_(std::move(topic_name))
    , region_name_(shm_pubsub::detail::build_region_name(topic_name_))
    , running_(false)
    , canceled_(false)
    , poll_interval_ms_(2)
    , last_read_idx_(0)
    , subscriber_token_(make_token())
    , subscriber_index_(-1)
    , logger_(shm_pubsub::logger::default_logger)
    , user_callback_is_synchronous_(true)
    , synchronous_user_callback_([](const auto&) {})
    , callback_queue_depth_(64)
    , callback_thread_stop_(true)
  {}

  Subscriber_Impl::~Subscriber_Impl()
  {
    cancel();
  }

  bool Subscriber_Impl::isRunning() const
  {
    return running_;
  }

  std::string Subscriber_Impl::format_prefix_() const
  {
    std::ostringstream oss;
    oss << "topic='" << topic_name_ << "': ";
    return oss.str();
  }

  shm_pubsub::detail::SubscriberEntry* Subscriber_Impl::subscriber_entry_()
  {
    if (subscriber_index_ < 0 || region_.addr == nullptr)
      return nullptr;
    auto* header = static_cast<shm_pubsub::detail::ShmHeader*>(region_.addr);
    auto* subs = reinterpret_cast<shm_pubsub::detail::SubscriberEntry*>(
      reinterpret_cast<char*>(region_.addr) + shm_pubsub::detail::subscriber_table_offset_bytes());
    const std::size_t max_subs = static_cast<std::size_t>(header->max_subscribers);
    const std::size_t idx = static_cast<std::size_t>(subscriber_index_);
    if (idx >= max_subs)
      return nullptr;
    return &subs[idx];
  }

  bool Subscriber_Impl::ensure_registered_()
  {
    if (subscriber_index_ >= 0)
      return true;
    if (region_.addr == nullptr)
      return false;

    auto* header = static_cast<shm_pubsub::detail::ShmHeader*>(region_.addr);
    const std::size_t max_subs = static_cast<std::size_t>(header->max_subscribers);
    auto* subs = reinterpret_cast<shm_pubsub::detail::SubscriberEntry*>(
      reinterpret_cast<char*>(region_.addr) + shm_pubsub::detail::subscriber_table_offset_bytes());

    const std::uint64_t write_idx = shm_pubsub::detail::load_u64_acquire(&header->write_idx);

    for (std::size_t i = 0; i < max_subs; ++i)
    {
      std::uint64_t expected = 0;
      if (!shm_pubsub::detail::compare_exchange_u64_acq_rel(&subs[i].active, expected, 1))
        continue;

      subs[i].token = subscriber_token_;
      shm_pubsub::detail::store_u64_release(&subs[i].read_idx, write_idx);
      shm_pubsub::detail::store_u64_release(&subs[i].lost_count, 0);
      shm_pubsub::detail::store_u64_release(&subs[i].last_seen_ns, now_ns());

      subscriber_index_ = static_cast<std::int32_t>(i);
      last_read_idx_ = write_idx;
      return true;
    }

    if (logger_)
      logger_(shm_pubsub::logger::LogLevel::Error, format_prefix_() + "no free subscriber slots (max_subscribers reached)");
    return false;
  }

  void Subscriber_Impl::unregister_if_needed_()
  {
    auto* entry = subscriber_entry_();
    if (!entry)
      return;
    const std::uint64_t token = shm_pubsub::detail::load_u64_acquire(&entry->token);
    if (token == subscriber_token_)
    {
      entry->token = 0;
      shm_pubsub::detail::store_u64_release(&entry->active, 0);
    }
    subscriber_index_ = -1;
  }

  shm_pubsub::detail::SlotHeader* Subscriber_Impl::slot_header_at_(std::uint64_t msg_idx)
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

  const char* Subscriber_Impl::slot_payload_at_(std::uint64_t msg_idx)
  {
    auto* slot = slot_header_at_(msg_idx);
    return reinterpret_cast<const char*>(slot) + sizeof(shm_pubsub::detail::SlotHeader);
  }

  void Subscriber_Impl::log_lost_(std::uint64_t lost)
  {
    if (lost == 0)
      return;
    if (logger_)
      logger_(shm_pubsub::logger::LogLevel::Warning, format_prefix_() + "lost messages (count=" + std::to_string(lost) + ")");
  }

  void Subscriber_Impl::stopCallbackThreadIfNeeded()
  {
    if (!callback_thread_)
      return;

    callback_thread_stop_ = true;
    last_callback_data_cv_.notify_all();

    if (callback_thread_->joinable())
    {
      if (std::this_thread::get_id() == callback_thread_->get_id())
        callback_thread_->detach();
      else
        callback_thread_->join();
    }

    callback_thread_.reset();
  }

  void Subscriber_Impl::setCallback(const std::function<void(const CallbackData&)>& callback_function, bool synchronous_execution)
  {
    const bool renew = (synchronous_execution || user_callback_is_synchronous_);

    if (synchronous_execution)
    {
      stopCallbackThreadIfNeeded();

      synchronous_user_callback_    = callback_function;
      user_callback_is_synchronous_ = true;

      const std::unique_lock<std::mutex> lock(last_callback_data_mutex_);
      callback_queue_.clear();
      return;
    }

    // async
    synchronous_user_callback_    = [](const auto&) {};
    user_callback_is_synchronous_ = false;

    // restart callback thread
    stopCallbackThreadIfNeeded();

    callback_thread_stop_ = false;
    callback_thread_ = std::make_unique<std::thread>(
      [me = shared_from_this(), callback_function]()
      {
        me->callbackLoop(callback_function);
      });

    if (renew)
    {
      // nothing to do, kept for symmetry with tcp_pubsub API
    }
  }

  void Subscriber_Impl::clearCallback()
  {
    setCallback([](const auto&) {}, true);
  }

  void Subscriber_Impl::start(std::uint32_t poll_interval_ms)
  {
    if (running_)
      return;

    poll_interval_ms_ = poll_interval_ms ? poll_interval_ms : 1;
    canceled_ = false;

    poll_thread_ = std::make_unique<std::thread>([me = shared_from_this()]() { me->pollLoop(); });
    running_ = true;
  }

  void Subscriber_Impl::cancel()
  {
    if (!running_)
      return;

    canceled_ = true;

    if (poll_thread_ && poll_thread_->joinable())
      poll_thread_->join();
    poll_thread_.reset();

    shm_pubsub::detail::close_region(region_);
    region_ = {};

    stopCallbackThreadIfNeeded();

    synchronous_user_callback_    = [](const auto&) {};
    user_callback_is_synchronous_ = true;
    running_ = false;
  }

  void Subscriber_Impl::callbackLoop(std::function<void(const CallbackData&)> callback_function)
  {
    for (;;)
    {
      CallbackData data;
      {
        std::unique_lock<std::mutex> lock(last_callback_data_mutex_);
        last_callback_data_cv_.wait(lock, [this]() { return !callback_queue_.empty() || callback_thread_stop_; });
        if (callback_thread_stop_)
          return;
        data = std::move(callback_queue_.front());
        callback_queue_.pop_front();
      }
      callback_function(data);
    }
  }

  void Subscriber_Impl::pollLoop()
  {
    while (!canceled_)
    {
      if (region_.addr == nullptr)
      {
        region_ = shm_pubsub::detail::open_existing(region_name_);
        if (region_.addr == nullptr)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(200));
          continue;
        }

        auto* header = static_cast<shm_pubsub::detail::ShmHeader*>(region_.addr);
        if (header->magic != shm_pubsub::detail::k_magic || header->version != shm_pubsub::detail::k_version)
        {
          shm_pubsub::detail::close_region(region_);
          region_ = {};
          std::this_thread::sleep_for(std::chrono::milliseconds(200));
          continue;
        }

        subscriber_index_ = -1;
        if (!ensure_registered_())
        {
          shm_pubsub::detail::close_region(region_);
          region_ = {};
          std::this_thread::sleep_for(std::chrono::milliseconds(200));
          continue;
        }
      }

      auto* header = static_cast<shm_pubsub::detail::ShmHeader*>(region_.addr);
      const std::size_t slot_count = static_cast<std::size_t>(header->slot_count);
      volatile std::uint64_t* write_idx_ptr = &header->write_idx;

      // Keep liveness hint fresh
      if (auto* entry = subscriber_entry_())
      {
        shm_pubsub::detail::store_u64_release(&entry->last_seen_ns, now_ns());

        const std::uint64_t active = shm_pubsub::detail::load_u64_acquire(&entry->active);
        const std::uint64_t token  = shm_pubsub::detail::load_u64_acquire(&entry->token);
        if (active == 0 || token != subscriber_token_)
        {
          subscriber_index_ = -1;
          if (!ensure_registered_())
          {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
          }
        }

        const std::uint64_t forced_read_idx = shm_pubsub::detail::load_u64_acquire(&entry->read_idx);
        if (forced_read_idx > last_read_idx_)
          last_read_idx_ = forced_read_idx;
      }

      std::uint64_t write_idx = shm_pubsub::detail::load_u64_acquire(write_idx_ptr);
      if (write_idx == last_read_idx_)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms_));
        continue;
      }

      // Drain multiple messages per wakeup to avoid artificial backpressure.
      bool progressed = false;
      for (std::size_t drained = 0; drained < 64 && !canceled_; ++drained)
      {
        write_idx = shm_pubsub::detail::load_u64_acquire(write_idx_ptr);
        if (write_idx == last_read_idx_)
          break;

        // If we lag too far behind, some messages are already overwritten
        const std::uint64_t lag = write_idx - last_read_idx_;
        if (lag > static_cast<std::uint64_t>(slot_count))
        {
          const std::uint64_t new_read = write_idx - static_cast<std::uint64_t>(slot_count);
          const std::uint64_t lost = new_read - last_read_idx_;
          last_read_idx_ = new_read;
          if (auto* entry = subscriber_entry_())
            shm_pubsub::detail::fetch_add_u64_acq_rel(&entry->lost_count, lost);
          log_lost_(lost);
        }

        const std::uint64_t next = last_read_idx_ + 1;

        // Try to read a consistent slot snapshot
        auto* slot = slot_header_at_(next);
        volatile std::uint64_t* slot_seq_ptr  = &slot->seq;
        volatile std::uint64_t* slot_size_ptr = &slot->size;

        std::shared_ptr<std::vector<char>> buffer = std::make_shared<std::vector<char>>();
        bool ok = false;
        std::uint64_t got_idx = 0;
        for (int i = 0; i < 3; ++i)
        {
          const std::uint64_t s1 = shm_pubsub::detail::load_u64_acquire(slot_seq_ptr);
          if ((s1 % 2) != 0)
            continue;

          const std::uint64_t msg_idx = slot->msg_idx;
          if (msg_idx < next)
            continue;
          const std::uint64_t size = shm_pubsub::detail::load_u64_acquire(slot_size_ptr);
          const std::size_t slot_size = static_cast<std::size_t>(header->slot_size_bytes);
          if (size > slot_size)
            break;

          buffer->resize(static_cast<std::size_t>(size));
          const char* payload_base = slot_payload_at_(next);
          if (size != 0)
            std::memcpy(buffer->data(), payload_base, static_cast<std::size_t>(size));

          const std::uint64_t s2 = shm_pubsub::detail::load_u64_acquire(slot_seq_ptr);
          if (s1 == s2 && (s2 % 2) == 0)
          {
            got_idx = msg_idx;
            ok = true;
            break;
          }
        }

        if (!ok)
          break;

        if (got_idx > next)
        {
          const std::uint64_t lost = got_idx - next;
          if (auto* entry = subscriber_entry_())
            shm_pubsub::detail::fetch_add_u64_acq_rel(&entry->lost_count, lost);
          log_lost_(lost);
        }

        if (user_callback_is_synchronous_)
        {
          CallbackData data;
          data.buffer_ = buffer;
          synchronous_user_callback_(data);
        }
        else
        {
          if (callback_thread_)
          {
            const std::lock_guard<std::mutex> lock(last_callback_data_mutex_);
            CallbackData data;
            data.buffer_ = buffer;
            if (callback_queue_.size() >= callback_queue_depth_)
            {
              callback_queue_.pop_front();
              if (logger_)
                logger_(shm_pubsub::logger::LogLevel::Warning, format_prefix_() + "callback queue full, drop oldest buffered callback");
            }
            callback_queue_.push_back(std::move(data));
            last_callback_data_cv_.notify_all();
          }
        }

        last_read_idx_ = std::max(last_read_idx_, got_idx);
        if (auto* entry = subscriber_entry_())
        {
          for (;;)
          {
            const std::uint64_t cur = shm_pubsub::detail::load_u64_acquire(&entry->read_idx);
            if (cur >= last_read_idx_)
              break;
            std::uint64_t expected = cur;
            if (shm_pubsub::detail::compare_exchange_u64_acq_rel(&entry->read_idx, expected, last_read_idx_))
              break;
          }
        }

        progressed = true;
      }

      if (!progressed)
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms_));
    }

    unregister_if_needed_();
  }
}
