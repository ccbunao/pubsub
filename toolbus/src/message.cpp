#include <toolbus/message.h>

#include <chrono>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace toolbus
{
  static std::uint64_t now_ms()
  {
    return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
  }

  static nlohmann::json to_json(const ToolCall& c)
  {
    nlohmann::json j;
    j["type"] = c.type;
    j["id"] = c.id;
    j["name"] = c.name;
    j["args"] = c.args;
    j["timeout_ms"] = c.timeout_ms;
    j["caller"] = c.caller;
    if (!c.target.empty()) j["target"] = c.target;
    if (!c.trace_id.empty()) j["trace_id"] = c.trace_id;
    j["ts_ms"] = c.ts_ms ? c.ts_ms : now_ms();
    return j;
  }

  static nlohmann::json to_json(const ToolResult& r)
  {
    nlohmann::json j;
    j["type"] = r.type;
    j["id"] = r.id;
    j["name"] = r.name;
    j["ok"] = r.ok;
    j["result"] = r.result;
    if (!r.ok)
    {
      nlohmann::json e;
      e["code"] = r.error.code;
      e["message"] = r.error.message;
      j["error"] = e;
    }
    else
    {
      j["error"] = nullptr;
    }
    j["duration_ms"] = r.duration_ms;
    j["caller"] = r.caller;
    j["worker"] = r.worker;
    if (!r.trace_id.empty()) j["trace_id"] = r.trace_id;
    j["ts_ms"] = r.ts_ms ? r.ts_ms : now_ms();
    return j;
  }

  std::string to_json_string(const ToolCall& call)
  {
    return to_json(call).dump();
  }

  std::string to_json_string(const ToolResult& res)
  {
    return to_json(res).dump();
  }

  ToolCall parse_tool_call(const std::string& json)
  {
    auto j = nlohmann::json::parse(json);
    ToolCall c;
    c.type = j.value("type", "tool_call");
    c.id = j.value("id", "");
    c.name = j.value("name", "");
    c.args = j.value("args", nlohmann::json::object());
    c.timeout_ms = j.value("timeout_ms", 2000);
    c.caller = j.value("caller", "");
    c.target = j.value("target", "");
    c.trace_id = j.value("trace_id", "");
    c.ts_ms = j.value("ts_ms", 0ULL);
    if (c.id.empty() || c.name.empty() || c.caller.empty())
      throw std::runtime_error("invalid ToolCall: missing id/name/caller");
    return c;
  }

  ToolResult parse_tool_result(const std::string& json)
  {
    auto j = nlohmann::json::parse(json);
    ToolResult r;
    r.type = j.value("type", "tool_result");
    r.id = j.value("id", "");
    r.name = j.value("name", "");
    r.ok = j.value("ok", false);
    r.result = j.value("result", nlohmann::json());
    if (!r.ok && j.contains("error") && !j["error"].is_null())
    {
      r.error.code = j["error"].value("code", "");
      r.error.message = j["error"].value("message", "");
    }
    r.duration_ms = j.value("duration_ms", 0);
    r.caller = j.value("caller", "");
    r.worker = j.value("worker", "");
    r.trace_id = j.value("trace_id", "");
    r.ts_ms = j.value("ts_ms", 0ULL);
    if (r.id.empty() || r.name.empty() || r.caller.empty())
      throw std::runtime_error("invalid ToolResult: missing id/name/caller");
    return r;
  }
}

