// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include <shm_pubsub/shm/subscriber.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

namespace
{
  std::atomic<bool> g_stop{false};

  void on_signal(int)
  {
    g_stop = true;
  }
}

int main()
{
  std::signal(SIGINT,  on_signal);
  std::signal(SIGTERM, on_signal);

  const std::string topic_name = "hello_world";

  shm_pubsub::shm::Subscriber subscriber(topic_name);
  subscriber.setCallback(
    [](const shm_pubsub::CallbackData& callback_data)
    {
      if (callback_data.buffer_)
      {
        const std::string msg(callback_data.buffer_->data(), callback_data.buffer_->size());
        std::cout << msg << "\n";
      }
    }
  );

  subscriber.start(2);

  std::cout << "Subscribing on topic '" << topic_name << "' (Ctrl+C to exit)\n";
  while (!g_stop)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  subscriber.cancel();
  return 0;
}

