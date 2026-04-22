// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include <shm_pubsub/shm/publisher.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

int main()
{
  const std::string topic_name = "hello_world";

  shm_pubsub::shm::Publisher publisher(topic_name, 1024 * 1024);
  if (!publisher.isRunning())
  {
    std::cerr << "Failed to create shared memory publisher for topic '" << topic_name << "'\n";
    return 1;
  }

  std::cout << "Publishing on topic '" << topic_name << "'\n";

  std::uint64_t counter = 0;
  for (;;)
  {
    const std::string payload = "Hello world " + std::to_string(counter++);
    publisher.send(payload.data(), payload.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

