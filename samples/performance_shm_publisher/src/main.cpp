// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include <shm_pubsub/shm/publisher.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
  std::atomic<std::uint64_t> g_sent_ok{0};
  std::atomic<std::uint64_t> g_sent_fail{0};

  void printLog()
  {
    for (;;)
    {
      const auto ok   = g_sent_ok.exchange(0);
      const auto fail = g_sent_fail.exchange(0);
      std::cout << "Sent ok=" << ok << " fail=" << fail << " in 1 second\n";
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  shm_pubsub::shm::OverflowPolicy parse_policy(const std::string& s)
  {
    if (s == "drop_newest" || s == "DropNewest" || s == "newest")
      return shm_pubsub::shm::OverflowPolicy::DropNewest;
    return shm_pubsub::shm::OverflowPolicy::DropOldest;
  }
}

int main(int argc, char** argv)
{
  const std::string topic = (argc >= 2) ? argv[1] : "performance";
  const std::size_t payload_bytes = (argc >= 3) ? static_cast<std::size_t>(std::stoul(argv[2])) : (64U * 1024U);
  const std::uint32_t interval_ms = (argc >= 4) ? static_cast<std::uint32_t>(std::stoul(argv[3])) : 0U;
  const auto policy = (argc >= 5) ? parse_policy(argv[4]) : shm_pubsub::shm::OverflowPolicy::DropOldest;

  shm_pubsub::shm::PublisherOptions opt;
  opt.max_subscribers = 8;
  opt.slot_size_bytes = std::max<std::size_t>(payload_bytes, 256U);
  opt.capacity_bytes  = 32U * 1024U * 1024U;
  opt.overflow_policy = policy;

  shm_pubsub::shm::Publisher publisher(topic, opt);
  if (!publisher.isRunning())
  {
    std::cerr << "Failed to create shared memory publisher for topic '" << topic << "'\n";
    return 1;
  }

  std::vector<char> payload(payload_bytes, 0);

  std::cout << "Publishing shm topic='" << topic
            << "' payload_bytes=" << payload_bytes
            << " interval_ms=" << interval_ms
            << " policy=" << ((policy == shm_pubsub::shm::OverflowPolicy::DropOldest) ? "drop_oldest" : "drop_newest")
            << "\n";

  std::thread print_thread(printLog);
  print_thread.detach();

  std::uint64_t counter = 0;
  for (;;)
  {
    // Write a counter to avoid overly-optimistic optimizations when debugging.
    if (payload.size() >= sizeof(counter))
      std::memcpy(payload.data(), &counter, sizeof(counter));

    const bool ok = publisher.send(payload.data(), payload.size());
    if (ok)
      g_sent_ok.fetch_add(1);
    else
      g_sent_fail.fetch_add(1);

    ++counter;

    if (interval_ms != 0)
      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
  }
}
