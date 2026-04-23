#pragma once

#include <string>

namespace toolbus
{
  // Generates a random ID suitable for request/response correlation.
  // Format: 32 hex chars (lowercase).
  std::string make_call_id();
}

