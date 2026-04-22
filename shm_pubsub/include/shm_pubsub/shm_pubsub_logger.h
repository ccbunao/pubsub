// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#pragma once

#include <functional>
#include <iostream>
#include <string>

namespace shm_pubsub
{
  namespace logger
  {
    enum class LogLevel
    {
      Debug,
      Info,
      Warning,
      Error,
    };

    using logger_t = std::function<void(const LogLevel, const std::string&)>;

    static const logger_t default_logger =
      [](const LogLevel level, const std::string& message)
      {
        switch (level)
        {
        case LogLevel::Debug:
          std::cout << "[SHM ps] [Debug]   " << message << "\n";
          break;
        case LogLevel::Info:
          std::cout << "[SHM ps] [Info]    " << message << "\n";
          break;
        case LogLevel::Warning:
          std::cerr << "[SHM ps] [Warning] " << message << "\n";
          break;
        case LogLevel::Error:
          std::cerr << "[SHM ps] [Error]   " << message << "\n";
          break;
        default:
          break;
        }
      };
  }
}

