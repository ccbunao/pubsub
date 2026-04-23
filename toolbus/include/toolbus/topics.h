#pragma once

#include <string>

namespace toolbus
{
  inline std::string call_topic(const std::string& service)
  {
    return std::string("tool.call.") + service;
  }

  inline std::string result_topic(const std::string& caller)
  {
    return std::string("tool.result.") + caller;
  }

  inline std::string event_topic(const std::string& caller)
  {
    return std::string("tool.event.") + caller;
  }
}

