// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace shm_pubsub::detail
{
  struct ShmRegion
  {
    void*       addr  = nullptr;
    std::size_t size  = 0;
    bool        owner = false;
    std::string name;

#if defined(_WIN32)
    void* handle = nullptr;
#else
    int fd = -1;
#endif
  };

  ShmRegion create_or_open(const std::string& name, std::size_t size);
  ShmRegion open_existing(const std::string& name);
  void close_region(ShmRegion& region);
}
