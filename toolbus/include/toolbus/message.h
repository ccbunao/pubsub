#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace toolbus
{
  struct ToolCall
  {
    std::string type = "tool_call";
    std::string id;
    std::string name;
    nlohmann::json args;
    std::uint32_t timeout_ms = 2000;
    std::string caller;
    std::string target;
    std::string trace_id;
    std::uint64_t ts_ms = 0;
  };

  struct ToolError
  {
    std::string code;
    std::string message;
  };

  struct ToolResult
  {
    std::string type = "tool_result";
    std::string id;
    std::string name;
    bool ok = false;
    nlohmann::json result;
    ToolError error;
    std::uint32_t duration_ms = 0;
    std::string caller;
    std::string worker;
    std::string trace_id;
    std::uint64_t ts_ms = 0;
  };

  // Serialize/parse helpers
  std::string to_json_string(const ToolCall& call);
  std::string to_json_string(const ToolResult& res);
  ToolCall parse_tool_call(const std::string& json);
  ToolResult parse_tool_result(const std::string& json);
}
