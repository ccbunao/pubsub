// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#pragma once

#include <string>

namespace shm_pubsub::detail
{
  inline std::string sanitize_topic(std::string topic)
  {
    if (topic.empty())
      return "default";

    for (auto& c : topic)
    {
      const bool ok =
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        (c == '_') || (c == '-') || (c == '.') ;
      if (!ok)
        c = '_';
    }
    return topic;
  }

  inline std::string build_region_name(const std::string& topic_name)
  {
    return std::string("shm_pubsub_") + sanitize_topic(topic_name);
  }
}
