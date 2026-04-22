// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#pragma once

#include <memory>
#include <vector>

namespace shm_pubsub
{
  struct CallbackData
  {
    std::shared_ptr<std::vector<char>> buffer_;
  };
}
