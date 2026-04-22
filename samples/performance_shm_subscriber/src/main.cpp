// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include <shm_pubsub/shm/subscriber.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

namespace
{
  std::atomic<std::uint64_t> g_received{0};

  void printLog()
  {
    for (;;)
    {
      const auto got = g_received.exchange(0);
      std::cout << "Received " << got << " in 1 second\n";
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

int main(int argc, char** argv)
{
  const std::string topic = (argc >= 2) ? argv[1] : "performance";
  const std::uint32_t poll_interval_ms = (argc >= 3) ? static_cast<std::uint32_t>(std::stoul(argv[2])) : 1U;

  shm_pubsub::shm::Subscriber subscriber(topic);
  subscriber.setCallback(
    [](const shm_pubsub::CallbackData& /*data*/)
    {
      g_received.fetch_add(1);
    },
    true /*synchronous*/
  );
  subscriber.start(poll_interval_ms);

  std::cout << "Subscribing shm topic='" << topic << "' poll_interval_ms=" << poll_interval_ms << "\n";

  std::thread print_thread(printLog);
  print_thread.detach();

  for (;;)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

